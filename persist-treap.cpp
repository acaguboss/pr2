#include <memory>
#include <iostream>
#include <iomanip>
#include <tuple>
using namespace std;

template <typename Tkey, typename Tval>
struct PersistTreap {
    private:

        struct Node {
            Tkey key;
            Tval val;
            int p;
            shared_ptr<Node> left;
            shared_ptr<Node> right;
        };

        shared_ptr<Node> root;

        //------------------------------------------------------------------------------------------------------------
        // Split
        //------------------------------------------------------------------------------------------------------------

        //A recursive implementation of split that is almost a direct translation of the pseudocode.
        //It uses a tuple (http://www.cplusplus.com/reference/tuple/tuple/) to return multiple values.
        //It also uses tie (http://www.cplusplus.com/reference/tuple/tie/) to destruct the return tuple.

        static tuple<shared_ptr<Node>, shared_ptr<Node>, shared_ptr<Node>> split_rec(shared_ptr<Node> v, Tkey key) {
            //returns a tuple (t1, t2, a) where t1 is a root of a treap of all elements smaller than key,
            //t2 is the root of a treap of all elements larger than key, and a is a node equal to key.

            if (!v) {
                return make_tuple(v, v, v);
            
            } else if (v->key == key) {
                //We have found the node to split around: set the output parameters instead of returning them.
                return make_tuple(v->left, v->right, v);

            } else if (key < v->key) {
                //key is somewhere to the left of v

                shared_ptr<Node> r1, r2, a;
                tie(r1, r2, a) = split_rec(v->left, key);

                //create a clone of v
                shared_ptr<Node> vclone = make_shared<Node>();
                vclone->key = v->key;
                vclone->val = v->val;
                vclone->p = v->p;
                vclone->left = r2;
                vclone->right = v->right;

                return make_tuple(r1, vclone, a);
                
            } else {
                //key is somewhere to the right of v
                shared_ptr<Node> r1, r2, a;
                tie(r1, r2, a) = split_rec(v->right, key);

                //create a clone of v
                shared_ptr<Node> vclone = make_shared<Node>();
                vclone->key = v->key;
                vclone->val = v->val;
                vclone->p = v->p;
                vclone->left = v->left;
                vclone->right = r1;

                return make_tuple(vclone, r2, a);
            }
        }

        //I spent some time trying to make the above recursive split tail-recursive in a way that g++ could optimize.
        //I eventually succeeded, but the main difficulty is the use of shared_ptrs.  Each shared_ptr has a destructor and the C++
        //standard says that destructors are run at the end of the block.  The execution of this destructor therefore happens after
        //the recursive call, making the recursive call NOT a tail-call since there is work after the recursion returns.  I worked
        //around this with a combination of raw pointers and switching between shared and raw pointers, but it got really messy.
        //
        //So instead, I decided it would be much cleaner to manually convert split into a loop.  Essentially manually performing
        //the tail-call optimization.  Here is the result.
        
        static tuple<shared_ptr<Node>, shared_ptr<Node>, shared_ptr<Node>> split_loop(shared_ptr<Node> v, Tkey key) {
            //result1, result2, and a hold the final result we will return, where result1 is the treap of stuff smaller than key,
            //result2 is the treap of stuff larger than key, and a is a node potentially equal to key.
            shared_ptr<Node> result1, result2, a;

            //t1 and t2 are pointers to shared_ptrs.  When the recursive version above would return t1 and t2,
            //instead the loop below will set the shared_ptr into the memory pointed to by t1 or t2.
            shared_ptr<Node> *t1, *t2;
           
            //Initially, t1 should point at result1 and t2 should point at result2
            t1 = &result1;
            t2 = &result2;

            //Loop until v is nil
            while (v) {

                if (v->key == key) {
                    //If v equals key, set the shared_ptr pointed to by t1 to v->left and
                    //set the shared_ptr pointed to by t2 to v->right.
                    *t1 = v->left;
                    *t2 = v->right;
                    a = v;
                    break;

                } else if (key < v->key) {

                    //First, create a clone of v
                    shared_ptr<Node> vclone = make_shared<Node>();
                    vclone->key = v->key;
                    vclone->val = v->val;
                    vclone->p = v->p;
                    vclone->right = v->right;

                    //vclone is the root of a treap of elements below v with values
                    //larger than x, so the vclone shared_ptr should be set in the memory pointed
                    //to by t2
                    *t2 = vclone;

                    //Now set the t2 pointer to point to the memory inside vclone holding the left shared_ptr.
                    //What will then happen is that the next time *t2 is set to something, that shared_ptr
                    //will be set into the memory inside the Node struct holding left.
                    t2 = &vclone->left;

                    //descend to the left
                    v = v->left;

                } else {
                    //make a clone of v
                    shared_ptr<Node> vclone = make_shared<Node>();
                    vclone->key = v->key;
                    vclone->val = v->val;
                    vclone->p = v->p;
                    vclone->left = v->left;

                    //vclone is the root of a treap of stuff below v which is smaller than key, so is set
                    //into the memory pointed to by t1.
                    *t1 = vclone;

                    //Set t1 so the next time *t1 is set it becomes the right child of vclone
                    t1 = &vclone->right;

                    //descend to the right
                    v = v->right;
                }
            }

            return make_tuple(result1, result2, a);
        }

        //A helper function allowing you to switch between the recursive or the loop version.
        static tuple<shared_ptr<Node>, shared_ptr<Node>, shared_ptr<Node>> split(shared_ptr<Node> v, Tkey key) {
            //return split_rec(v, key);
            return split_loop(v, key);
        }

        //------------------------------------------------------------------------------------------------------------
        // Join
        //------------------------------------------------------------------------------------------------------------

        static shared_ptr<Node> join(shared_ptr<Node> v1, shared_ptr<Node> v2) {
            //join takes as input two nodes v1 and v2, where the elements in v1 have smaller keys than elements
            //in v2.  There is return value is the new root of a treap consisting of the union of the elements
            //from v1 and v2.

            //If either v1 or v2 is nil, set the return value to be the other
            if (!v1) {
                return v2;

            } else if (!v2) {
                return v1;

            } else if (v1->p < v2->p) {
                //make a clone of v1 since v1 will become the new root
                shared_ptr<Node> w = make_shared<Node>();
                w->key = v1->key;
                w->val = v1->val;
                w->p = v1->p;

                //the left child is unchanged
                w->left = v1->left;

                //the right child is joined to v2
                w->right = join(v1->right, v2);

                return w;

            } else {
                //make a clone of v2 since v2 will become the new root
                shared_ptr<Node> w = make_shared<Node>();
                w->key = v2->key;
                w->val = v2->val;
                w->p = v2->p;

                //the right child is unchanged
                w->right = v2->right;

                //the left child is joined to v1
                w->left = join(v1, v2->left);

                return w;
            }
        }

        //The above join is not tail-recursive.  Just like split, it can be made tail-recursive but the
        //shared_ptr destructors make it complicated.  Instead, in my opinion the best way of optimizing join
        //is to explicitly convert it to a loop, like I did in split_loop.

        //------------------------------------------------------------------------------------------------------------
        // Union
        //------------------------------------------------------------------------------------------------------------

        static shared_ptr<Node> union_helper(shared_ptr<Node> v1, shared_ptr<Node> v2) {
            //union takes as input two nodes v1 and v2 and returns a node for the left-biased union

            //If either v1 or v2 is nil, 
            if (!v1) {
                return v2;

            } else if (!v2) {
                return v1;

            } else if (v1->p < v2->p) {
                //v1 will become the new root of the union, so split v2 around
                //v1->key, getting t1, t2, and a as output
                shared_ptr<Node> t1, t2, a;
                tie(t1, t2, a) = split(v2, v1->key);

                //create a clone of v1
                shared_ptr<Node> w = make_shared<Node>();
                w->key = v1->key;
                w->val = v1->val;
                w->p = v1->p;
                w->left = union_helper(v1->left, t1);
                w->right = union_helper(v1->right, t2);
                return w;

            } else {
                //v2 will become the new root of the union, so split v1
                //around v2->key, getting t1, t2, and a as output
                shared_ptr<Node> t1, t2, a;
                tie(t1, t2, a) = split(v1, v2->key);

                //clone of v2
                shared_ptr<Node> w = make_shared<Node>();
                w->key = v2->key;
                if (a) {
                    //to be left-biased, use the value from v1
                    w->val = a->val;
                } else {
                    //v2->key does not exist in v1, so use v2->val;
                    w->val = v2->val;
                }
                w->p = v2->p;
                w->left = union_helper(t1, v2->left);
                w->right = union_helper(t2, v2->right);
                return w;
            }
        }

        //------------------------------------------------------------------------------------------------------------
        // Intersection
        //------------------------------------------------------------------------------------------------------------

        static shared_ptr<Node> intersect_helper(shared_ptr<Node> v1, shared_ptr<Node> v2) {
            //intersect_helper takes as input two nodes v1 and v2 and returns a new node for the intersection
            //of v1 and v2.  The values from v1 are used.
            
            if (!v1 || !v2) {
                return NULL;
            
            } else if (v1->p < v2->p) {
                //v1 is potentially the new root, so split v2 around v1->key
                shared_ptr<Node> r1, r2, a;
                tie(r1, r2, a) = split(v2, v1->key);

                //intersect the stuff smaller than v1->key
                shared_ptr<Node> left = intersect_helper(v1->left, r1);
                //intersect the stuff larger than v1->key
                shared_ptr<Node> right = intersect_helper(v1->right, r2);

                if (!a) {
                    //v1->key does not exist in v2, so v1 should not appear in the output.
                    //Instead, join left and right
                    return join(left, right);
                } else {
                    //make a clone of v1
                    shared_ptr<Node> w = make_shared<Node>();
                    w->key = v1->key;
                    w->val = v1->val;
                    w->p = v1->p;
                    w->left = left;
                    w->right = right;
                    return w;
                }

            } else {
                //v2 is potentially the new root, so split v1 around v2->key
                shared_ptr<Node> r1, r2, a;
                tie(r1, r2, a) = split(v1, v2->key);

                //intersect the stuff smaller and larger than v2->key
                shared_ptr<Node> left = intersect_helper(r1, v2->left);
                shared_ptr<Node> right = intersect_helper(r2, v2->right);

                if (!a) {
                    //v2->key is not in v1, so join left and right ignoring v2 itself
                    return join(left, right);
                } else {
                    //make a clone of v2
                    shared_ptr<Node> w = make_shared<Node>();
                    w->key = v2->key;
                    w->val = a->val; //use the value from v1 so the intersect is left-biased
                    w->p = v2->p;
                    w->left = left;
                    w->right = right;
                    return w;
                }
            }
        }

        //------------------------------------------------------------------------------------------------------------
        // Difference
        //------------------------------------------------------------------------------------------------------------

        static shared_ptr<Node> difference_helper(shared_ptr<Node> v1, shared_ptr<Node> v2) {
            //TODO: you write this
	    if (!v1) {
                return NULL;
            } else if (!v2) {
                return v1;	
            } else {//used to be (v1->p < v2->p)
                //v1 is potentially the new root, so split v2 around v1->key
                shared_ptr<Node> r1, r2, a;
                tie(r1, r2, a) = split(v2, v1->key);

                //intersect the stuff smaller than v1->key
                shared_ptr<Node> left = difference_helper(v1->left, r1);
                //intersect the stuff larger than v1->key
                shared_ptr<Node> right = difference_helper(v1->right, r2);

                if (!a) {
                    //v1->key does not exist in v2, so v1 should appear in the output.
		    //make a clone of v1
                    shared_ptr<Node> w = make_shared<Node>();
                    w->key = v1->key;
                    w->val = v1->val;
                    w->p = v1->p;
                    w->left = left;
                    w->right = right;
                    return w;
                } else {
		    //v1 exists in v2
                    //join left and right
                     return join(left, right);
                }
            }
        }

        //------------------------------------------------------------------------------------------------------------
        // Debug print
        //------------------------------------------------------------------------------------------------------------

        static void debug_print_helper(shared_ptr<Node> v, int indent) {
            //print the tree via a pre-order traversal.  We print the node, then print
            //its children at one larger indent level.
            if (v) {
                if (indent) {
                    cout << setw(indent) << " ";
                }
                cout << v->key << ":" << v->p << ":" << v->val << endl;
                debug_print_helper(v->left, indent+4);
                debug_print_helper(v->right, indent+4);
            }
        }

    public:

        //A constructor that initializes the treap to the empty treap using the fact that
        //the root shared_ptr is initialized to nil by the shared_ptr constructor.
        PersistTreap() {}

        //A constructor to construct a tree consisting of a single node.
        PersistTreap(Tkey key, Tval val) {
            root = make_shared<Node>();
            root->key = key;
            root->val = val;
            root->p = rand() ;//%1000; // added % 100; for easier reading of p values
        }

        static PersistTreap treap_union(PersistTreap treap1, PersistTreap treap2) {
            //call union_helper, unwrapping and then re-wrapping the PersistTree into a shared_ptr<Node>
            PersistTreap newTreap;
            newTreap.root = union_helper(treap1.root, treap2.root);
            return newTreap;
        }

        static PersistTreap intersection(PersistTreap treap1, PersistTreap treap2) {
            //call intersect_helper, unwrapping and then re-wrapping the PersistTree into a shared_ptr<Node>
            PersistTreap newTreap;
            newTreap.root = intersect_helper(treap1.root, treap2.root);
            return newTreap;
        }

        static PersistTreap difference(PersistTreap treap1, PersistTreap treap2) {
            //TODO: you write this
	    PersistTreap newTreap;
            newTreap.root = difference_helper(treap1.root, treap2.root);
            return newTreap;
        }

        PersistTreap insert(Tkey key, Tval val) {
            return treap_union(PersistTreap(key, val), *this);
        }

        PersistTreap remove(Tkey key) {
            return difference(*this, PersistTreap(key, NULL));
        }

        void debug_print() {
            debug_print_helper(root, 0);
        }
};

int main() {
    srand(time(0));

    //create a treap of the first 0 through 39
    cout << "Treap 1" << endl;
    PersistTreap<int, int> treap1;
    for (int i = 0; i < 10; i++) {
        //insert is persistent so returns the new treap with the value inserted.  Just replace
        //the treap1 variable with the return value and discard the old treap.
        treap1 = treap1.insert(i, i*10);
    }
    treap1.debug_print();

    //treap2 contains even numbers from 0 to 78
    cout << endl << "Treap 2" << endl;
    PersistTreap<int, int> treap2;
    for (int i = 0; i < 30; i += 2) {
        treap2 = treap2.insert(i, i*1000);
    }
    treap2.debug_print();

    //treap 3 is the union.  Notice how the left-biased values come from treap1 and how some memory
    //locations are shared.
    cout << endl << "Treap 3 = union of treap 1 and 2" << endl;
    PersistTreap<int, int> treap3 = PersistTreap<int, int>::treap_union(treap1, treap2);
    treap3.debug_print();

    //now print treap1 again to see that it is unchanged even though nodes are shared between treap1 and treap3
    cout << endl << "Treap 1 again" << endl;
    treap1.debug_print();

    //treap 4 is the intersection
    cout << endl << "Treap 4 = intersection of treap 1 and 2" << endl;
    PersistTreap<int, int> treap4 = PersistTreap<int, int>::intersection(treap1, treap2);
    treap4.debug_print();
    

    //TODO: call difference and print the result
    cout << endl << "Treap 5 = difference of treap 1 and 2" << endl;
    PersistTreap<int, int> treap5 = PersistTreap<int, int>::difference(treap1, treap2);
    treap5.debug_print();
    
    //now print treap1 again to see that it is unchanged even though nodes are shared between treap1 and treap3
    cout << endl << "Treap 1 again" << endl;
    treap1.debug_print();
}
