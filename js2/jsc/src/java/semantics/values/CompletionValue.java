package com.compilercompany.es3c.v1;

/**
 * A completion value (intermediate).
 */

public class CompletionValue extends Value {

    private static final boolean debug = false;

    int type;
    Value value;
    String target;

    public CompletionValue(int type, Value value, String target) {
        this.type   = type;
        this.value  = value;
        this.target = target;
    }

    public CompletionValue () {
        this(CompletionType.normalType,NullValue.nullValue,"");
    }

    public String toString() {
        return "completion( "+type+", "+value+", "+target+" )";
    }

}

/*
 * The end.
 */
