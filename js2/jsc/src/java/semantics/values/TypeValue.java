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
import java.util.Enumeration;

/**
 * class TypeValue
 *
 * TypeValues have an internal property valueType that is
 * the type that this value represents. The type of a TypeValue
 * is always ObjectType.type.
 */

public class TypeValue extends ObjectValue implements Type {

    private static final boolean debug = false;
    private String name;
    private Type impl;  // This is the implementation of this value object.

    /**
     *
     */

    public TypeValue( Type impl ) {
        //super(valueType);
        //TypeType.type.addSub(valueType);
        this.impl = impl;
        this.type = TypeType.type;
    }

    /**
     * The root type value.
     */

    public TypeValue() {
        //super(impl);
        //TypeType.type.addSub(impl);
        this.impl = null;
        this.type = null;
    }

    /**
     *
     */

    public String toString() {
        return "typevalue(" + impl + ")" + slots;
    }

    // Type methods

    /**
     *
     */

    public Value convert(Context context, Value value) {
        return this;
    }

    /**
     *
     */

    public Value coerce(Context context, Value value) {
        return this;
    }

    /**
     *
     */

    public void addSub(Type type) {
        if( impl != null ) {
            impl.addSub(type);        
        } else {
            // Otherwise, this is the root type so we already
            // know that this is a subtype, since all types are
            // subtypes.
        }
    }

    /**
     *
     */

    public void setSuper(Type superClass) {
        impl.setSuper(superClass);
    }

    /**
     *
     */

    public Type getSuper() {
        return impl.getSuper();
    }

    /**
     *
     */

    private Vector values_   = new Vector();
    private Type[] converts_ = null;

    public Object[] values() {
        Object[] values = new Object[values_.size()];
        values_.copyInto(values);
        return values;
    }

    /**
     *
     */

    public Type[] converts() {
        return converts_;
    }

    /**
     * Init
     *
     * Initialize a raw value. This is how class objects
     * construct instances. This is the second of three
     * steps taken to create a new object.
     *
     * 1 allocate raw value.
     * 2 call this method.
     * 3 call user constructor, if one exists.
     *
     * For each instance member in the type definition, 
     * add a slot to the Value ob.
     */

    public void init( Value ob ) {
    }

    /**
     *
     */

    public boolean includes(Value value) {

        if( debug ) {
            Debugger.trace("includes() type = " + impl + ", value = "+value);
        }

        boolean result;
        
        if( this.impl==null ) {
            if( value.type == TypeType.type ) {
                result = true;
            } else {
                result = false;
            }
        } else {
            result = this.impl.includes(value);
        }

        if( debug ) {
            Debugger.trace(">> result = " + result);
        }

        return result;

    };

}

/*
 * The end.
 */
