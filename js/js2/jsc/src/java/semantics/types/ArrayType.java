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
