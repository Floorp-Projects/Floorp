/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * Bojan Cekrlic
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

// API class

package org.mozilla.javascript;

/**
 * Java reflection of JavaScript exceptions.
 * Instances of this class are thrown by the JavaScript 'throw' keyword.
 *
 * @author Mike McCabe
 */
public class JavaScriptException extends Exception
{
    /**
     * @deprecated
     * Use {@link EvaluatorException#EvaluatorException(String)} to report
     * exceptions in Java code.
     */
    public JavaScriptException(Object value)
    {
        this(value, "", 0);
    }

    /**
     * Create a JavaScript exception wrapping the given JavaScript value
     *
     * @param value the JavaScript value thrown.
     */
    public JavaScriptException(Object value, String sourceName, int lineNumber)
    {
        super(EvaluatorException.generateErrorMessage(
                toMessage(value), sourceName, lineNumber));
        this.lineNumber = lineNumber;
        this.sourceName = sourceName;
        this.value = value;
    }

    /**
     * @return the value wrapped by this exception
     */
    public Object getValue()
    {
        return value;
    }

    /**
     * Get the name of the source containing the error, or null
     * if that information is not available.
     */
    public String getSourceName()
    {
        return sourceName;
    }

    /**
     * Returns the line number of the statement causing the error,
     * or zero if not available.
     */
    public int getLineNumber()
    {
        return lineNumber;
    }

    private static String toMessage(Object object)
    {
        if (object instanceof Scriptable) {
            // to prevent potential of evaluation and throwing more exceptions
            return ScriptRuntime.defaultObjectToString((Scriptable)object);
        }
        return ScriptRuntime.toString(object);
    }

    private Object value;
    private String sourceName;
    private int lineNumber;
}
