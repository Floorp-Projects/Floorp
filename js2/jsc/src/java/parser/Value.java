package com.compilercompany.es3c.v1;
import java.util.Hashtable;
import java.util.Vector;

/**
 * The value class from which all other values derive.
 */

abstract public class Value implements Scope {

    private static final boolean debug = false;

    public Value type;
    public Value getValue(Context context) throws Exception { return this; }
    public Value getType(Context context) throws Exception { return type; }
    
/*
    Hashtable defaultNames = new Hashtable();
    Hashtable namespaces   = new Hashtable();
    Hashtable attributes   = new Hashtable();
*/

    public boolean has(Value namespace, String name) throws Exception {
        throw new Exception("Constructor object expected in new expression");
/*
        Hashtable names;
        if( namespace == null ) {
            names = defaultNames;
        } else {
            names = (Hashtable) namespaces.get(namespace);
        }
        return names.containsKey(name);
*/
    }

    public Slot get(Value namespace, String name) throws Exception {
        throw new Exception("Constructor object expected in new expression");
/*
        if(debug) {
            Debugger.trace("ObjectValue.get() with namespaces="+namespaces+", namespace="+namespace+", name="+name);
        }
        Hashtable names;
        if( namespace == null ) {
            names = defaultNames;
        } else {
            names = (Hashtable) namespaces.get(namespace);
        }
        return (Slot) names.get(name);
*/
    }

    public Slot add(Value namespace, String name) throws Exception {
        throw new Exception("Constructor object expected in new expression");
/*
        if( debug ) {
            Debugger.trace("ObjectType.add() with this = " + this + " namespace = " + namespace + " name = " + name);
        }

        Hashtable names;
        if( namespace == null ) {
            names = defaultNames;
        } else {
            names = (Hashtable) namespaces.get(namespace);
            if( names==null ) {
                names = new Hashtable();
                namespaces.put(namespace,names);
            }
        }

        Slot slot  = new Slot();
        names.put(name,slot);
        return slot;
*/
    }

    int size() {
	    return 0;
	}

    public Value construct(Context context, Value args) throws Exception {
        throw new Exception("Constructor object expected in new expression");
    }

    public Value call(Context context, Value args) throws Exception {
        throw new Exception("Callable object expected in call expression");
    }

}

/*
 * The end.
 */
