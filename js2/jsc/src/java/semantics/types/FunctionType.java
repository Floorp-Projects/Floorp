package com.compilercompany.es3c.v1;

/**
 * class FunctionType
 */

public final class FunctionType extends ObjectType implements Type {

    boolean debug = false;
  
    public static final TypeValue type = new TypeValue(new FunctionType());

    String name;

    public String toString() {
        return "function("+name+")";
    }

    public FunctionType(String name) {
	    super("function");
        this.name = name;
        FunctionType.type.addSub(this);
    }

    private FunctionType() {
	    super("function");
        ObjectType.type.addSub(this);
    }

}

/*
 * The end.
 */
