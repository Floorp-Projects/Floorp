package com.compilercompany.es3c.v1;

/**
 * The base Node class.
 */

public class Node {

    private static final boolean debug = false;

    Block block;
    int position;
	Store store;

    public Node() {
    }

    public Node( int position ) {
        this.position = position;
    }

    public Value evaluate( Context context, Evaluator evaluator ) throws Exception {
        return null;
    }
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
    public Node first() {
        return this;
    }
    public Node last() {
        return this;
    }
    public Node pos(int p) {
        position = p;
        return this;
    }
    public int pos() {
        return position;
    }
    public String toString() {
        return isLeader ? "*"+block+":" : ""+block+":";
    }
}

/*
 * The end.
 */
