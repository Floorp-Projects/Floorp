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
 * This is interface that all functions in JavaScript must implement.
 * The interface provides for calling functions and constructors.
 *
 * @see org.mozilla.javascript.Scriptable
 * @author Norris Boyd
 */

public interface Function extends Scriptable {
    /**
     * Call the function.
     *
     * Note that the array of arguments is not guaranteed to have
     * length greater than 0.
     *
     * @param cx the current Context for this thread
     * @param scope the scope to execute the function relative to. This
     *              set to the value returned by getParentScope() except
     *              when the function is called from a closure.
     * @param thisObj the JavaScript <code>this</code> object
     * @param args the array of arguments
     * @return the result of the call
     * @exception JavaScriptException if an uncaught exception
     *            occurred while executing the function
     */
    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException;

    /**
     * Call the function as a constructor.
     *
     * This method is invoked by the runtime in order to satisfy a use
     * of the JavaScript <code>new</code> operator.  This method is
     * expected to create a new object and return it.
     *
     * @param cx the current Context for this thread
     * @param scope the scope to execute the function relative to. This
     *              set to the value returned by getParentScope() except
     *              when the function is called from a closure.
     * @param args the array of arguments
     * @return the allocated object
     * @exception JavaScriptException if an uncaught exception
     *            occurred while executing the constructor
     */
    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
        throws JavaScriptException;
}
