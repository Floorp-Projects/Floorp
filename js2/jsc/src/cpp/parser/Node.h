/**
 * The base Node class.
 */

#ifndef Node_h
#define Node_h

namespace esc {
namespace v1 {

class Evaluator;
class Value;
class Context;

struct Node {

    int position;
	char local_var_index;

	Node() {
    }

    Node( int position ) {
        this->position = position;
    }

    virtual Value* evaluate( Context& cx, Evaluator* evaluator ) {
        return (Value*) 0;
    }

    Node* first() {
        return this;
    }

    Node* last() {
        return this;
    }

    Node* pos(int p) {
        position = p;
        return this;
    }

    int pos() {
        return position;
    }

#if 0

    private static final boolean debug = false;

    Block block;
	Store store;

    boolean isLeader;
    public void markLeader() throws Exception {

        if( debug ) {
            Debugger.trace( "Node.markLeader() with this = " + this );
            //if(this instanceof AnnotatedBlockNode) throw new Exception("blah");
        }

        isLeader=true;
    }
    public boolean isLeader() {
        return isLeader;
    }
    public boolean isBranch() {
        return false;
    }
    public Node[] getTargets() {
        return null;
    }
    public String toString() {
        return isLeader ? "*"+block+":" : ""+block+":";
    }
#endif

};

}
}

#endif // Node_h

/*
 * The end.
 */
