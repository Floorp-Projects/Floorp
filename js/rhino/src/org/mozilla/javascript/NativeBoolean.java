/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

package org.mozilla.javascript;

/**
 * This class implements the Boolean native object.
 * See ECMA 15.6.
 * @author Norris Boyd
 */
public class NativeBoolean extends ScriptableObject {

    /**
     * Zero-parameter constructor: just used to create Boolean.prototype
     */
    public NativeBoolean() {
    }

    public NativeBoolean(boolean b) {
        booleanValue = b;
    }

    public String getClassName() {
        return "Boolean";
    }

    public Object getDefaultValue(Class typeHint) {
        // This is actually non-ECMA, but will be proposed
        // as a change in round 2.
        if (typeHint == ScriptRuntime.BooleanClass)
	    return booleanValue ? Boolean.TRUE : Boolean.FALSE;
        return super.getDefaultValue(typeHint);
    }

    public static Object Boolean(Context cx, Object[] args, Function ctorObj,
                                 boolean inNewExpr)
    {
        boolean b = args.length >= 1
                    ? ScriptRuntime.toBoolean(args[0])
                    : false;
    	if (inNewExpr) {
    	    // new Boolean(val) creates a new boolean object.
    	    return new NativeBoolean(b);
    	}

    	// Boolean(val) converts val to a boolean.
    	return b ? Boolean.TRUE : Boolean.FALSE;
    }

    public String toString() {
	    return booleanValue ? "true" : "false";
    }

    public boolean valueOf() {
	    return booleanValue;
    }

    private boolean booleanValue;
}
