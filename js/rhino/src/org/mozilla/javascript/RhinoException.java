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
 * Igor Bukanov
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
public abstract class RhinoException extends RuntimeException
{
    RhinoException()
    {
    }

    RhinoException(String details)
    {
        super(details);
    }

    public final String getMessage()
    {
        String details = details();
        if (sourceName == null || lineNumber <= 0) {
            return details;
        }
        StringBuffer buf = new StringBuffer(details);
        buf.append(" (");
        if (sourceName != null) {
            buf.append(sourceName);
        }
        if (lineNumber > 0) {
            buf.append('#');
            buf.append(lineNumber);
        }
        buf.append(')');
        return buf.toString();
    }

    public String details()
    {
        return super.getMessage();
    }

    /**
     * Get the uri of the script source containing the error, or null
     * if that information is not available.
     */
    public final String sourceName()
    {
        return sourceName;
    }

    /**
     * Initialize the uri of the script source containing the error.
     *
     * @param sourceName the uri of the script source reponsible for the error.
     *                   It should not be <tt>null</tt>.
     *
     * @throws IllegalStateException if the method is called more then once.
     */
    public final void initSourceName(String sourceName)
    {
        if (sourceName == null) throw new IllegalArgumentException();
        if (this.sourceName != null) throw new IllegalStateException();
        this.sourceName = sourceName;
    }

    /**
     * Returns the line number of the statement causing the error,
     * or zero if not available.
     */
    public final int lineNumber()
    {
        return lineNumber;
    }

    /**
     * Initialize the line number of the script statement causing the error.
     *
     * @param lineNumber the line number in the script source.
     *                   It should be positive number.
     *
     * @throws IllegalStateException if the method is called more then once.
     */
    public final void initLineNumber(int lineNumber)
    {
        if (lineNumber <= 0) throw new IllegalArgumentException(String.valueOf(lineNumber));
        if (this.lineNumber > 0) throw new IllegalStateException();
        this.lineNumber = lineNumber;
    }

    /**
     * The column number of the location of the error, or zero if unknown.
     */
    public final int columnNumber()
    {
        return columnNumber;
    }

    /**
     * Initialize the column number of the script statement causing the error.
     *
     * @param columnNumber the column number in the script source.
     *                     It should be positive number.
     *
     * @throws IllegalStateException if the method is called more then once.
     */
    public final void initColumnNumber(int columnNumber)
    {
        if (columnNumber <= 0) throw new IllegalArgumentException(String.valueOf(columnNumber));
        if (this.columnNumber > 0) throw new IllegalStateException();
        this.columnNumber = columnNumber;
    }

    /**
     * The source text of the line causing the error, or null if unknown.
     */
    public final String lineSource()
    {
        return lineSource;
    }

    /**
     * Initialize the text of the source line containing the error.
     *
     * @param lineSource the text of the source line reponsible for the error.
     *                   It should not be <tt>null</tt>.
     *
     * @throws IllegalStateException if the method is called more then once.
     */
    public final void initLineSource(String lineSource)
    {
        if (lineSource == null) throw new IllegalArgumentException();
        if (this.lineSource != null) throw new IllegalStateException();
        this.lineSource = lineSource;
    }

    final void recordErrorOrigin(String sourceName, int lineNumber,
                                 String lineSource, int columnNumber)
    {
        // XXX: for compatibility allow for now -1 to mean 0
        if (lineNumber == -1) {
            lineNumber = 0;
        }

        if (sourceName != null) {
            initSourceName(sourceName);
        }
        if (lineNumber != 0) {
            initLineNumber(lineNumber);
        }
        if (lineSource != null) {
            initLineSource(lineSource);
        }
        if (columnNumber != 0) {
            initColumnNumber(columnNumber);
        }
    }

    private String sourceName;
    private int lineNumber;
    private String lineSource;
    private int columnNumber;
}
