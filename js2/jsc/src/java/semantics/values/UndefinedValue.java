package com.compilercompany.es3c.v1;

/**
 * The undefined value.
 */

public class UndefinedValue extends ObjectValue {

    private static final boolean debug          = false;
    public  static final Value   undefinedValue = new UndefinedValue();

    public UndefinedValue () {
        this.type = UndefinedType.type;
    }

    public String toString() {
        return "undefined";
    }

}

/*
 * The end.
 */
