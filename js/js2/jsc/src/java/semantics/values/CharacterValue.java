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
 * class CharacterValue
 */

public class CharacterValue extends ObjectValue {

    private static final boolean debug = false;

    public String toString() {
        return "character("+value+")";
    }

    char value;

    CharacterValue(char value) {
        this.type  = CharacterType.type;
        this.value = value;
    }

    CharacterValue(char value, Value type) {
        this.type  = type;
        this.value = value;
    }

    public Value getValue(Context context) throws Exception { 

        Value value;

        if(type == CharacterType.type) {
            value = this;
        } else {
            throw new Exception("Unexpected type of character value.");
        }
    
        return value; 
    }
    
}

/*
 * The end.
 */
