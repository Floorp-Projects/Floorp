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

// API class

package org.mozilla.javascript;

/**
 * All compiled scripts implement this interface.
 * <p>
 * This class encapsulates script execution relative to an
 * object scope.
 * @since 1.3
 * @author Norris Boyd
 */

public interface Script {

    /**
     * Execute the script.
     * <p>
     * The script is executed in a particular runtime Context, which
     * must be associated with the current thread.
     * The script is executed relative to a scope--definitions and
     * uses of global top-level variables and functions will access
     * properties of the scope object. For compliant ECMA
     * programs, the scope must be an object that has been initialized
     * as a global object using <code>Context.initStandardObjects</code>.
     * <p>
     *
     * @param cx the Context associated with the current thread
     * @param scope the scope to execute relative to
     * @return the result of executing the script
     * @see org.mozilla.javascript.Context#initStandardObjects
     * @exception JavaScriptException if an uncaught JavaScript exception
     *            occurred while executing the script
     */
    public Object exec(Context cx, Scriptable scope)
        throws JavaScriptException;

}
