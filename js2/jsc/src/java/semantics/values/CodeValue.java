package com.compilercompany.es3c.v1;
import java.util.Vector;

/**
 * A code value (intermediate).
 *
 * This value holds the intermediate code for an non-constant value.
 */

public class CodeValue extends Value {

    private static final boolean debug = false;
    private Node node;

    public CodeValue( Node node ) {
        this.node = node;
    }

    public String toString() {
        return "code("+node+")";
    }

    /**
     * The result depends on during what phase this function
     * is called. At compile time it will either return itself,
     * or a compile-time constant value. At runtime it will 
     * return a runtime value.
     */

    public Value getValue(Context context) throws Exception {

        // ACTION: execute the code to get the value. 

        return node.evaluate(context,context.evaluator); 
    }
}

/*
 * The end.
 */
