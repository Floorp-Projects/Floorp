package com.compilercompany.es3c.v1;

/**
 * class IntegerType
 */

public final class IntegerType extends ObjectType implements Type {

    boolean debug = false;
  
    public static final TypeValue type = new TypeValue(new IntegerType());

    public String toString() {
        return "integer";
    }

    private IntegerType() {
	    super("integer");
        ObjectType.type.addSub(this);
    }

}

/*
 * The end.
 */
