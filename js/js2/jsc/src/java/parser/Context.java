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
import java.util.Stack;
import java.util.Vector;
import java.util.Enumeration;
import java.util.Hashtable;

/**
 * Context used during semantic analysis.
 */

public class Context {

    private static final boolean debug = true;

    Evaluator evaluator;
    Stack     scopeStack;
    Stack     classStack;    
    Scope     global;
    Hashtable predecessors = new Hashtable();
    Hashtable D;



    int indent = 0;
    int errors = 0;
    public int errorCount() {
        return errors;
    }

    String getIndent() {
        StringBuffer str = new StringBuffer();
        for (int i = 0; i < indent; i++) {
            str.append('\t');
        }
        return str.toString();
    }

    /**
     *
     */

    static InputBuffer in;

    public static InputBuffer getInput() {
        return in;
    }

    public static void init(InputBuffer input) {
        in=input;
    }

    /**
     *
     */

    public Evaluator getEvaluator() {
        return evaluator;
    }

    public void setEvaluator(Evaluator evaluator) {
        this.evaluator=evaluator;
    }

    /**
     * The bottom of the stack.
     */

    Scope getGlobal() {
        return global; 
    }

    /**
     * The top of the scope stack.
     */

    Scope getLocal() { 
        return (Scope) scopeStack.peek(); 
    }              
    
    /**
     *  Make the given scope the new innermost scope.
     */

    public Scope pushScope(Scope scope) { 
        if( scopeStack==null ) {
            scopeStack = new Stack();
            global = scope;
        }
        scopeStack.push(scope);
        return scope; 
    }

    /**
     *  Pop the top scope off the stack.
     */

    public void popScope() { 
        if( scopeStack==null ) {
            return;
        }
        scopeStack.pop();
        return;
    }

    /**
     *  Make the given scope the new innermost scope.
     */

    void enterClass(Scope scope) {
        pushScope(scope);
        if( classStack==null ) {
            classStack = new Stack();
        }
        classStack.push(scope);
        return;
    }

    void exitClass() { 
        classStack.pop();
        popScope();
        return;
    }

    Scope getThisClass() {
        if(classStack.size()==0) {
            return global;
        } else { 
            return (Scope) classStack.peek(); 
        }
    }              
    
    /**
     * The immediate outer.
     */
    
    Scope nextScope(Scope scope) {
        Scope next;
        int index = scopeStack.indexOf(scope);
        if(index==0) {
            next = null;
        } else {
            next = (Scope) scopeStack.elementAt(index-1);
        }
        return next; 
    }

    private Block thisBlock;
    Vector blocks = new Vector();

    int blockCount;
    void enterBlock(Node entry) throws Exception {
        if( debug ) {
            Debugger.trace("Context.enterBlock() with entry = " + entry );
            //Thread.dumpStack();
            //if(entry instanceof AnnotatedBlockNode) throw new Exception("blah");
        }
        if( thisBlock!=null ) {
            throw new Exception("Entering block before exiting previous block.");
        }
        thisBlock = new Block(blockCount++);
        thisBlock.setEntry(entry);
        blocks.addElement(thisBlock);
    }

    Block getBlock() {
        return thisBlock;
    }

    void setBlock(Block b) {
        thisBlock=b;
    }

    void exitBlock(Node exit) {
        if( debug ) {
            Debugger.trace("Context.exitBlock() with exit = " + exit );
        }
        if(thisBlock!=null) {
            thisBlock.setExit(exit);
            thisBlock = null;
        }
    }

    /**
     * Get all basic blocks in the current program.
     */

    public Vector getBlocks() {
        return blocks;
    }

    /**
     * Add an edge.
     */

    void addEdge(Block b1, Block b2) {
	    if( debug ) {
		    Debugger.trace("Context.addEdge() with b1 = " + b1 + ", b2 = " + b2 );
		}
        Vector preds = (Vector) predecessors.get(b2);
        if( preds == null ) {
            preds = new Vector();
            preds.addElement(b1);
            predecessors.put(b2,preds);
        } else if( !preds.contains(b1) ) {
            preds.addElement(b1);
        }
	}

    public String toString() { return "context( " + scopeStack + ", " + global + " )"; }
}

/*
 * The end.
 */
