package com.compilercompany.es3c.v1;
import java.util.Vector;
import java.util.Enumeration;

/**
 * A list value (intermediate).
 */

public class ListValue extends ObjectValue {

    private static final boolean debug = false;

    Vector data = new Vector();

    int size() {
        return data.size();
    }

    Enumeration elements() {
        return data.elements();
    }

    public String toString() {
        return "list(" + data + ")";
    }

    public ListValue push(Object item) {
        data.addElement(item);
        return this;
    }

    public Value getValue(Context context) throws Exception {
        int size = data.size();
        if( size > 0 ) {
            return ((Value) data.elementAt(size-1)).getValue(context);
        }
        return UndefinedValue.undefinedValue;
    }

    public Value getValue(Context context, int n) throws Exception {
        int size = data.size();
        if( n >= 0 && n < size ) {
            return ((Value) data.elementAt(n)).getValue(context);
        }
        return UndefinedValue.undefinedValue;
    }

    public void pop() {
        int size = data.size();
        if( size > 0 ) {
            data.setSize(size-1);
        }
    }
}

/*
 * The end.
 */
