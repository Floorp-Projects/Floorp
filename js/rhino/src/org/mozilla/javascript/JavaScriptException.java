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

import java.lang.reflect.InvocationTargetException;

/**
 * Java reflection of JavaScript exceptions.  (Possibly wrapping a Java exception.)
 *
 * @author Mike McCabe
 */
public class JavaScriptException extends Exception {

    /**
     * Create a JavaScript exception wrapping the given JavaScript value.
     *
     * Instances of this class are thrown by the JavaScript 'throw' keyword.
     *
     * @param value the JavaScript value thrown.
     */
    public JavaScriptException(Object value) {
        super(ScriptRuntime.toString(value));
        this.value = value;
    }

    /**
     * Get the exception message.
     *
     * <p>Will just convert the wrapped exception to a string.
     */
    public String getMessage() {
        return ScriptRuntime.toString(value);
    }

    static JavaScriptException wrapException(Scriptable scope,
                                             Throwable exn)
    {
        if (exn instanceof InvocationTargetException)
            exn = ((InvocationTargetException)exn).getTargetException();
        if (exn instanceof JavaScriptException)
            return (JavaScriptException)exn;
        Object wrapper = NativeJavaObject.wrap(scope, exn, Throwable.class);
        return new JavaScriptException(wrapper);
    }

    /**
     * Get the exception value originally thrown.  This may be a
     * JavaScript value (null, undefined, Boolean, Number, String,
     * Scriptable or Function) or a Java exception value thrown from a
     * host object or from Java called through LiveConnect.
     *
     * @return the value wrapped by this exception
     */
    public Object getValue() {
        if (value != null && value instanceof Wrapper)
            // this will also catch NativeStrings...
            return ((Wrapper)value).unwrap();
        else
            return value;
    }

    /**
     * The JavaScript exception value.  This value is not
     * intended for general use; if the JavaScriptException wraps a
     * Java exception, getScriptableValue may return a Scriptable
     * wrapping the original Java exception object.
     *
     * We would prefer to go through a getter to encapsulate the value,
     * however that causes the bizarre error "nanosecond timeout value 
     * out of range" on the MS JVM. 
     * @serial 
     */
    Object value;
}
