/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mountain View Compiler
 * Company.  Portions created by Mountain View Compiler Company are
 * Copyright (C) 1998-2000 Mountain View Compiler Company. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Jeff Dyer <jeff@compilercompany.com>
 */

package com.compilercompany.ecmascript;

/**
 * The base Node class.
 */

public class Node {

    private static final boolean debug = false;

    Block block;
    int position;

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
