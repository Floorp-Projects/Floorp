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
import java.util.Vector;

/**
 * class ObjectType
 */

public class ObjectType implements Type {

    private static final boolean debug = true;

    public static final TypeValue type = new TypeValue(new ObjectType());

    String name = "object";

    public String toString() {
        return name;
    }

    public ObjectType(String name) {
        this.name = name;
        ObjectType.type.addSub(this);
    }

    private ObjectType() {
    }

  
    /**
     * Type methods
     */

    /**
     * Init
     */

    public void init( Value ob ) throws Exception {
    }

    private Vector   values_   = new Vector();
    private Type[]   converts_ = null;

    public Type[] converts() {
        if( converts_ == null) {
            converts_    = new Type[6];
            converts_[0] = UndefinedType.type;
            converts_[1] = NullType.type;
            converts_[2] = BooleanType.type;
            converts_[3] = NumberType.type;
            converts_[4] = StringType.type;
            converts_[5] = ObjectType.type;
        }
        return converts_;
    }

    public void addSub(Type type) {
        if( debug ) {
            Debugger.trace("ObjectType.addSub() type = " + type + " to " + this);
        }
        values_.addElement(type);        
    }

    public Object[] values() {
        Object[] values = new Object[values_.size()];
        values_.copyInto(values);
        return values;
    }

    public Value convert(Context context, Value value) throws Exception {
        return value;
    }

    public Value coerce(Context context, Value value) {
        return value;
    }

    private Type superType;

    public void setSuper(Type type) {
        superType = type;
    }

    public Type getSuper() {
        return superType;
    }

    public boolean includes(Value value) {

	    if( debug ) {
		    Debugger.trace("ObjectType.includes() with this = " + this + ", value.type = " + value.type + ", values_ = " + values_);
		}

        return values_.contains(value.type);
    }


    /**
     * subset
     *
     * return t2 if t2 is a proper subset of t1, otherwise
     * return null.
     */

    public static Type subset(Type t1, Type t2) {

        if(debug) {
            Debugger.trace("subset() with t1 = " + t1 + ", t2 = " + t2 );
        }

        Object[] v1 = t1.values();
        Object[] v2 = t2.values();

        int      l1 = v1.length;
        int      l2 = v2.length;
                
        int      i1;
        int      i2;

        for(i2 = 0; i2 < l2; i2++) {

            // For each value in t2, check that it is also in t1. 

            for(i1 = 0; i1 < l1; i1++) {

                if(debug) {
                    Debugger.trace("subset() matching " + v2[i2] + " = " + v1[i1] );
                }

                // If the match is found, then break out of this
                // inner loop and continue checking values.

                if(v1[i1]==v2[i2]) {
                    if(debug) {
                        Debugger.trace("value found");
                    }
                    break;
                }
            }

            // Not a subset if we got to the end of the t1 value array
            // without finding the current t2 value.

            if(i1==l1) {
                return null;
            }
        }

        // If we got to the end of the t2 values, then t2 is a subset
        // of t1. Return t2.

        if(i2==l2) {
            return t2;
        }

        return null;
    }

    public static  Type subset(Type[] at1, Type t2) {
        
        if( at1 == null ) {
            return null;
        }

        // Check that the values of t2 are a subset of values
        // of at least one of the types of the ta1 array.

        int l = at1.length;
        int i;

        for( i = 0; i < l; i++ ) {
            if( subset(at1[i],t2) != null ) {
                return at1[i];
            }
        }

        return null;
    }


    /**
     * intersection
     *
     * return the aggregate type that represents the intersection
     * of t1 and t2. The empty set is expressed as null.
     */

    public static  Type intersection(Type t1, Type t2) {
        
        // Return a type that represents the intersection
        // of values in both t1 and t2.

        /*
        for(;;) {
            for(;;) {
            }
        }
        */

        return null;
    }

    public static  Type intersection(Type[] ts1, Type t2) {
        return null;
    }
}

/*
 * The end.
 */
