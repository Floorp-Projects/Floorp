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

/**
 * The class of exceptions thrown by the JavaScript engine.
 */
public class EvaluatorException extends RuntimeException
{
    /**
     * @deprecated Use 
     * {@link EvaluatorException(String detail, String sourceName, 
int lineNumber)}
     * to construct detailed error messages.
     */
    public EvaluatorException(String detail)
    {
        super(detail);
        Context cx = Context.getCurrentContext();
        if (cx!= null) {
            int[] linep = { 0 };
            this.sourceName = cx.getSourcePositionFromStack(linep);
            this.lineNumber = linep[0];
        }
    }

    /**
     * Create an exception with the specified detail message.
     *
     * Errors internal to the JavaScript engine will simply throw a
     * RuntimeException.
     *
     * @param detail the error message
     * @param sourceName the name of the source reponsible for the error
     * @param lineNumber the line number of the source
     */
    public EvaluatorException(String detail, String sourceName, 
                              int lineNumber)
    {
        this(detail, sourceName, lineNumber, null, 0);
    }

    /**
     * Create an exception with the specified detail message.
     *
     * Errors internal to the JavaScript engine will simply throw a
     * RuntimeException.
     *
     * @param detail the error message
     * @param sourceName the name of the source reponsible for the error
     * @param lineNumber the line number of the source
     * @param columnNumber the columnNumber of the source (may be zero if
     *                     unknown)
     * @param lineSource the source of the line containing the error (may be
     *                   null if unknown)
     */
    public EvaluatorException(String detail, String sourceName, int lineNumber,
                              String lineSource, int columnNumber)
    {
        super(generateErrorMessage(detail, sourceName, lineNumber));
        this.sourceName = sourceName;
        this.lineNumber = lineNumber;
        this.lineSource = lineSource;
        this.columnNumber = columnNumber;
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

    /**
     * The column number of the location of the error, or zero if unknown.
     */
    public int getColumnNumber()
    {
        return columnNumber;
    }

    /**
     * The source of the line causing the error, or zero if unknown.
     */
    public String getLineSource()
    {
        return lineSource;
    }

    static String generateErrorMessage(String message,
                                       String sourceName,
                                       int line)
    {
        if (sourceName == null || line <= 0) {
            return message;
        }
        StringBuffer buf = new StringBuffer(message);
        buf.append(" (");
        if (sourceName != null) {
            buf.append(sourceName);
        }
        if (line > 0) {
            buf.append('#');
            buf.append(line);
        }
        buf.append(')');
        return buf.toString();
    }

    private String sourceName;
    private int lineNumber;
    private String lineSource;
    private int columnNumber;
}
