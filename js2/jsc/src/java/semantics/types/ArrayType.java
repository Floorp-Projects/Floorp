package com.compilercompany.es3c.v1;
import java.util.Enumeration;

/**
 * class ArrayType
 */

public final class ArrayType extends ObjectType implements Type {

    boolean debug = false;
  
    public static final TypeValue type = new TypeValue(new ArrayType());

    public String toString() {
        return "array";
    }

    private ArrayType() {
	    super("array");
        ObjectType.type.addSub(this);
    }

    /**
     * Type methods
     */

    public Value convert(Context context, Value value) throws Exception {
        if( value instanceof ListValue ) {
            return this.convert(context,(ListValue)value);
        } else {
            return value;
        }
    }

    public Value convert(Context context, ListValue value) throws Exception {
        Value array = new ObjectValue(this);
        Enumeration e = value.elements();
        int i = 0;
        while(e.hasMoreElements()) {
            Slot slot;
            String name;
            Value elem;
            name = ""+i;
            slot = array.add(null,name);
            elem = (Value) e.nextElement();
            slot.value = elem.getValue(context);
            i++;
        }
        return array;
    }

}

/*
 * The end.
 */
