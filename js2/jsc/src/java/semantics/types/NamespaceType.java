package com.compilercompany.es3c.v1;

/**
 * class NamespaceType
 */

public final class NamespaceType extends ObjectType implements Type {

    boolean debug = false;
  
    public static final TypeValue type = new TypeValue(new NamespaceType());

    public String toString() {
        if( name == null ) {
            return "namespace";
        } else {
            return "namespace("+name+")";
        }
    }

    private String name;

    public NamespaceType(String name) {
	    super("namespace");
        this.name = name;
        NamespaceType.type.addSub(this);
    }

    private NamespaceType() {
	    super("namespace");
        ObjectType.type.addSub(this);
    }

}

/*
 * The end.
 */
