package com.compilercompany.es3c.v1;

/**
 * A number value.
 */

public final class NumberValue extends ObjectValue {

    boolean debug = false;
  
    String value;

    public NumberValue(double value) {
        this.type  = NumberType.type;
        this.value = null; // ACTION: value;
    }

    public NumberValue(String value) throws Exception {
        this.type  = NumberType.type;
        this.value = value;
    }
    
    public String toString() {
        return ""+value;
    }
}

/*
 * The end.
 */
