package com.compilercompany.es3c.v1;

/**
 * class CharacterType
 */

public final class CharacterType extends ObjectType implements Type {

    boolean debug = false;
  
    public static final TypeValue type = new TypeValue(new CharacterType());

    public String toString() {
        return "character";
    }

    private CharacterType() {
	    super("character");
        ObjectType.type.addSub(this);
    }

}

/*
 * The end.
 */
