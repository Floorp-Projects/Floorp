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
 * class CompletionValue
 */

public class CompletionValue extends Value {

    private static final boolean debug = false;

    int type;
    Value value;
    String target;

    public CompletionValue(int type, Value value, String target) {
        this.type   = type;
        this.value  = value;
        this.target = target;
    }

    public CompletionValue () {
        this(CompletionType.normalType,NullValue.nullValue,"");
    }

    public String toString() {
        return "completion( "+type+", "+value+", "+target+" )";
    }

}

/*
 * The end.
 */
