package com.compilercompany.es3c.v1;

/**
 * The null value.
 */

public class NullValue extends ObjectValue {

    private static final boolean debug = false;

    public static final Value nullValue = new NullValue();

    public String toString() {
        return "null";
    }

}

/*
 * The end.
 */
