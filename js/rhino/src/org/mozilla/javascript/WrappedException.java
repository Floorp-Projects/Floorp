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
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * A wrapper for runtime exceptions.
 *
 * Used by the JavaScript runtime to wrap and propagate exceptions that occur
 * during runtime.
 *
 * @author Norris Boyd
 */
public class WrappedException extends EvaluatorException
{
    /**
     * @see Context#throwAsScriptRuntimeEx(Throwable e)
     */
    public WrappedException(Throwable exception)
    {
        super("Wrapped "+exception.toString());
        this.exception = exception;
        Kit.initCause(this, exception);

        Context cx = Context.getCurrentContext();
        if (cx!= null) {
            int[] linep = { 0 };
            String sourceName = cx.getSourcePositionFromStack(linep);
            int lineNumber = linep[0];
            if (sourceName != null) {
                initSourceName(sourceName);
            }
            if (lineNumber != 0) {
                initLineNumber(lineNumber);
            }
        }
    }

    /**
     * Get the wrapped exception.
     *
     * @return the exception that was presented as a argument to the
     *         constructor when this object was created
     */
    public Throwable getWrappedException()
    {
        return exception;
    }

    /**
     * @deprecated Use {@link #getWrappedException()} instead.
     */
    public Object unwrap()
    {
        return getWrappedException();
    }

    private Throwable exception;
}
