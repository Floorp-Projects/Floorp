package com.compilercompany.es3c.v1;

/**
 * Represents a basic block for flow analysis.
 */

public class Block {
    Node entry, exit;

    int n;

    Block(int n) {
        this.n = n;
    }
    
    void setEntry( Node entry ) {
        this.entry = entry;
    }

    void setExit( Node exit ) {
        this.exit = exit;
    }

    public String toString() {
        return "B"+n/*+"( " + entry + ", " + exit + " )"*/;
    }
}

/*
 * The end.
 */
