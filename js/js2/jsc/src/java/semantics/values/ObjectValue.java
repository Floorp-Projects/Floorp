/* 
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mountain View Compiler
 * Company.  Portions created by Mountain View Compiler Company are
 * Copyright (C) 1998-2000 Mountain View Compiler Company. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Jeff Dyer <jeff@compilercompany.com>
 */

package com.compilercompany.ecmascript;
import java.util.Hashtable;
import java.util.Vector;
import java.util.Enumeration;

/**
 * An object value.
 */

public class ObjectValue extends Value implements Attributes, Scope {

    private static final boolean debug = false;

    /**
     *
     */

    Hashtable slots = new Hashtable();
    String name;

    /**
     *
     */

    public ObjectValue() {
	    this.type = null;
    }

    /**
     *
     */

    public ObjectValue( TypeValue type ) throws Exception {
        if( debug ) {
            Debugger.trace("ObjectValue.ObjectValue() with type = " + type);
        }
        type.init(this);
        this.type = type;
    }

    /**
     *
     */

    public ObjectValue( String name, TypeValue type ) throws Exception {
        type.init(this);
        this.type = type;
    }

    /**
     *
     */

    public ObjectValue( Init initializer ) throws Exception {
        this.type = ObjectType.type;
        initializer.init(this);
    }

    /**
     *
     */

    public ObjectValue( String name, Init initializer ) throws Exception {
        this.name = name;
        this.type = ObjectType.type;
        initializer.init(this);
    }

    /**
     *
     */

    public String toString() {
        
        if( name != null ) {
            return "object("+name+")"+slots;
        } else {
            return "object("+type+")"+slots;
        }
    }

    /**
     *
     */

    public Value getValue(Context context) throws Exception { 
        return this; 
    };

    /**
     *
     */

    public boolean has(Value namespace, String name) {
        return get(namespace,name) != null;
    }

    /**
     *
     */

    public Slot get(Value namespace, String name) {

        if( debug ) {
            Debugger.trace("ObjectValue.get() namespace="+namespace+", name="+name);
        }

        Slot slot;
        if( namespace == null ) {
            slot = (Slot) slots.get(name);
        } else {
            ObjectValue names = (ObjectValue)slots.get(namespace);
			if( names == null ) {
			    slot = null;
			} else {
                slot = (Slot) names.slots.get(name);
			}
        }

        return slot;
    }

    /**
     * Add a name to a namespace and return the resulting slot.
     */

    public Slot add(Value namespace, String name) throws Exception {
        if( debug ) {
            Debugger.trace("ObjectType.add() to " + this.name + " namespace = " + namespace + " name = " + name);
        }

        Hashtable slots;
        ObjectValue names;

        if( namespace == null ) {
            slots = this.slots;
        } else {
            names = (ObjectValue) this.slots.get(namespace);
            if( names==null ) {
                names = new ObjectValue(new NamespaceType(namespace.toString()));
                this.slots.put(namespace,names);
                slots = names.slots;
            } else {
                slots = names.slots;
            }
        }

        Slot slot  = new Slot();
        slot.type  = ObjectType.type;
        slot.value = UndefinedValue.undefinedValue;

        slots.put(name,slot);

        return slot;
    }

    /**
     *
     */

    public Value construct(Context context, Value args) throws Exception {

        Scope thisScope = context.getThisClass();

        if( debug ) {
            Debugger.trace("calling object constructor with this = " + thisScope + ", args = " + args);
        }

        return (Value) thisScope;
    }

    /**
     *
     */

    public Value call(Context context, Value args) throws Exception {

        Scope thisScope = context.getThisClass();

        if( debug ) {
            Debugger.trace("calling function with this = " + thisScope + ", args = " + args);
        }

        Value value = UndefinedValue.undefinedValue;

        return value;
    }

}

/*
 * The end.
 */
