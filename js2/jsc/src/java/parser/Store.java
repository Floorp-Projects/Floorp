package com.compilercompany.es3c.v1;

/**
 * Represents an abstract unit of storage.
 */

public class Store {
    Scope scope;
    int   index;

    Store(Scope scope, int index) {
        this.scope = scope;
        this.index = index;
    }

    public String toString() {
        return "store( " + index + " )";
    }
}

/*
 * The end.
 */
