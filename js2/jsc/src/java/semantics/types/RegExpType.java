package com.compilercompany.es3c.v1;

/**
 * class RegExpType
 */

public final class RegExpType extends ObjectType implements Type {

    boolean debug = false;
  
    public static final RegExpType type = new RegExpType();

    String expr;

    public String toString() {
        return "regexp("+expr+")";
    }

    private RegExpType() {
	    super("regexp");
        ObjectType.type.addSub(this);
    }

    public RegExpType(String expr) {
	    super("regexp");
        this.expr = expr;

        // Make this type a subtype of the base RegExpType.
        // This means that RegExpType.type.includes(this)
        // is true. Since values created by this type have
        // this type as their type, for any reg exp value
        // RegExpType.type.includes(value.type) is true.

        RegExpType.type.addSub(this);
    }

}

/*
 * The end.
 */
