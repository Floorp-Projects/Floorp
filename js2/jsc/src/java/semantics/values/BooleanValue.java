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
