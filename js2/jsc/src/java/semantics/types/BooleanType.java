package com.compilercompany.es3c.v1;

/**
 * class BooleanType
 */

public final class BooleanType extends ObjectType implements Type {

    private static final boolean debug = false;
  
    public static final TypeValue type = new TypeValue(new BooleanType());

    public String toString() {
        return "boolean";
    }

    private BooleanType() {
	    super("boolean");
        ObjectType.type.addSub(this);
    }

}

/*
 * The end.
 */
