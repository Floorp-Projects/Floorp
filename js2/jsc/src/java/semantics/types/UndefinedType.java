package com.compilercompany.es3c.v1;

/**
 * class UndefinedType
 */

public final class UndefinedType extends ObjectType implements Type {

    boolean debug = false;
  
    public static final TypeValue type = new TypeValue(new UndefinedType());

    public String toString() {
        return "undefined";
    }

    private UndefinedType() {
	    super("undefined");
        ObjectType.type.addSub(this);
    }
}

/*
 * The end.
 */
