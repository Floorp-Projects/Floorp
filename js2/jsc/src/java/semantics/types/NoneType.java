package com.compilercompany.es3c.v1;

/**
 * class NoneType
 */

public final class NoneType extends ObjectType implements Type {

    private static final boolean debug = false;
  
    public static final TypeValue type = new TypeValue(new NoneType());

    public String toString() {
        return "none";
    }

    private NoneType() {
	    super("none");
        ObjectType.type.addSub(this);
    }

}

/*
 * The end.
 */
