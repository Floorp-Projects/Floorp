package com.compilercompany.es3c.v1;

/**
 * class NullType
 */

public final class NullType extends ObjectType implements Type {

    boolean debug = false;
  
    public static final TypeValue type = new TypeValue(new NullType());

    public String toString() {
        return "null";
    }

    private NullType() {
	    super("null");
        ObjectType.type.addSub(this);
    }

}

/*
 * The end.
 */
