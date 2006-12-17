/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Norris Boyd
 *   Bojan Cekrlic
 *   Hannes Wallnoefer
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above. If
 * you wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replacing
 * them with the notice and other provisions required by the GPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */

// API class

package org.mozilla.javascript;

/**
 * Java reflection of JavaScript exceptions.
 * Instances of this class are thrown by the JavaScript 'throw' keyword.
 *
 * @author Mike McCabe
 */
public class JavaScriptException extends RhinoException
{
    static final long serialVersionUID = -7666130513694669293L;

    /**
     * @deprecated
     * Use {@link WrappedException#WrappedException(Throwable)} to report
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
        recordErrorOrigin(sourceName, lineNumber, null, 0);
        this.value = value;
    }

    public String details()
    {
       try {
           return ScriptRuntime.toString(value);
       } catch (RuntimeException rte) {
           // ScriptRuntime.toString may throw a RuntimeException
           if (value == null) {
               return "null";
           } else if (value instanceof Scriptable) {
               return ScriptRuntime.defaultObjectToString((Scriptable)value);
           } else {
               return value.toString();
           }
       }
    }

    /**
     * @return the value wrapped by this exception
     */
    public Object getValue()
    {
        return value;
    }

    /**
     * @deprecated Use {@link RhinoException#sourceName()} from the super class.
     */
    public String getSourceName()
    {
        return sourceName();
    }

    /**
     * @deprecated Use {@link RhinoException#lineNumber()} from the super class.
     */
    public int getLineNumber()
    {
        return lineNumber();
    }

    private Object value;
}
