package com.compilercompany.es3c.v1;

/**
 * A boolean value: true or false.
 */

public class BooleanValue extends ObjectValue {

    private static final boolean debug = false;

    public static final Value trueValue = new BooleanValue(true);
    public static final Value falseValue = new BooleanValue(false);

    boolean value;

    public String toString() {
        return "boolean("+value+")";
    }

    BooleanValue(boolean value) {
        this.value = value;
        this.type  = BooleanType.type;
    }

    BooleanValue(boolean value, Value type) {
        this.value = value;
        this.type  = type;
    }

    /**
     * Return a value of the currently specified type.
     */

    public Value getValue(Context context) throws Exception { 

        Value value;

        if(type == BooleanType.type) {
            value = this;
        } else {
            value = ((TypeValue)type).convert(context,this);
        }
    
        return value; 
    }
    
}

/*
 * The end.
 */
