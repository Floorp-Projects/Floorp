/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Norris Boyd
 * Mike McCabe
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
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

    public static Object jsConstructor(Context cx, Object[] args, 
                                       Function ctorObj, boolean inNewExpr)
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

    public String jsFunction_toString() {
	    return booleanValue ? "true" : "false";
    }

    public boolean jsFunction_valueOf() {
	    return booleanValue;
    }

    private boolean booleanValue;
}
