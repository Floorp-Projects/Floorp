package com.compilercompany.es3c.v1;

/**
 * A slot.
 */

public class Slot {
    Value attrs;
    Value type;
    Value value;
    Block block;
	Store store;

    public String toString() {
        return "{ "+attrs+", "+type+", "+value+", "+store+" }";
    }
}

/*
 * The end.
 */
