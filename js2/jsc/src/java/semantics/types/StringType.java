package com.compilercompany.es3c.v1;

/**
 * class StringType
 */

public final class StringType extends ObjectType implements Type {

    boolean debug = false;
  
    public static final TypeValue type = new TypeValue(new StringType());

    public String toString() {
        return "string";
    }

    private StringType() {
        super("string");
        ObjectType.type.addSub(this);
    }

}

/*
 * The end.
 */
