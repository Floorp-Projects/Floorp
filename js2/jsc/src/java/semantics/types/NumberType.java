package com.compilercompany.es3c.v1;

/**
 * class NumberType
 */

public final class NumberType extends ObjectType implements Type {

    boolean debug = false;
  
    public static final TypeValue type = new TypeValue(new NumberType());

    public String toString() {
        return "number";
    }

    private NumberType() {
	    super("number");
        ObjectType.type.addSub(this);
    }

    /**
     * Type methods
     */

    public void init( Value ob ) throws Exception {

        if( debug ) {
            Debugger.trace("NumberType.init() with ob = " + ob );
        }

        Slot slot;
        slot = ob.add(null,"Number");
        slot.type  = FunctionType.type;
        slot.value = new ObjectValue(new FunctionType("constructor"));
        slot.value.type = NumberType.type;
    }

}

/*
 * The end.
 */
