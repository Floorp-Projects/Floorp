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
 * class ListValue
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
