package com.compilercompany.es3c.v1;

/**
 * class TypeType
 */

public class TypeType extends ObjectType implements Type {

    boolean debug = false;
  
    public static TypeValue type = new TypeValue();

    public String toString() {
        return "type";
    }

    public TypeType() {
	    super("type");
        ObjectType.type.addSub(this);
    }

    public boolean includes(Value value) {

        // A type is always a member of itself.

        if(value.type==TypeType.type) {
           return true;
        }

        // Check the store in the base object.

        return super.includes(value);
    }
    
}

/*
 * The end.
 */
