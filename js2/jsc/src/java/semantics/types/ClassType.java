package com.compilercompany.es3c.v1;
import java.util.Vector;

/**
 * class ClassType
 */

public final class ClassType extends ObjectType implements Type {

    private static final boolean debug = false;
  
    public static final TypeValue type = new TypeValue(new ClassType());

    String name = "";

    public String toString() {
        return "class("+name+" extends " + superType + ")";
    }

    public ClassType(String name) {
	    super("class#"+name);
        this.name = name;
        ClassType.type.addSub(this);
    }

    private ClassType() {
	    super("class");
        ObjectType.type.addSub(this);
    }

    private Type superType;

    public void setSuper(Type type) {
        superType = type;
        superType.addSub(this);
    }

    public Type getSuper() {
        return superType;
    }

    /**
     * Init
     */

    public void init( Value ob ) {
        ob.type = TypeType.type;
    }
}

/*
 * The end.
 */
