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

/**
 * Value
 *
 * This is the value class from which all other values derive.
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
