package com.compilercompany.es3c.v1;

/**
 * A string value.
 */

public final class StringValue extends ObjectValue {

    boolean debug = false;
  
    String value;

    public StringValue(String value) {
        this.type  = StringType.type;
        this.value = value;
    }

    public String toString() {
        return value;
    }

}

/*
 * The end.
 */
