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
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
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

// API class

package org.mozilla.javascript.debug;

/**
 * This interface exposes debugging information from executable
 * code (either functions or top-level scripts).
 */
public interface DebuggableScript
{
    public boolean isTopLevel();

    /**
     * Returns true if this is a function, false if it is a script.
     */
    public boolean isFunction();

    /**
     * Get name of the function described by this script.
     * Return null or an empty string if this script is not function.
     */
    public String getFunctionName();

    /**
     * Get number of declared parameters in function.
     * Return 0 if this script is not function.
     *
     * @see #getParamAndVarCount()
     * @see #getParamOrVarName(int index)
     */
    public int getParamCount();

    /**
     * Get number of declared parameters and local variables.
     * Return number of declared global variables if this script is not
     * function.
     *
     * @see #getParamCount()
     * @see #getParamOrVarName(int index)
     */
    public int getParamAndVarCount();

    /**
     * Get name of a declared parameter or local variable.
     * <tt>index</tt> should be less then {@link #getParamAndVarCount()}.
     * If <tt>index&nbsp;&lt;&nbsp;{@link #getParamCount()}</tt>, return
     * the name of the corresponding parameter, otherwise return the name
     * of variable.
     * If this script is not function, return the name of the declared
     * global variable.
     */
    public String getParamOrVarName(int index);

    /**
     * Get the name of the source (usually filename or URL)
     * of the script.
     */
    public String getSourceName();

    /**
     * Returns true if this script or function were runtime-generated
     * from JavaScript using <tt>eval</tt> function or <tt>Function</tt>
     * or <tt>Script</tt> constructors.
     */
    public boolean isGeneratedScript();

    /**
     * Get array containing the line numbers that
     * that can be passed to <code>DebugFrame.onLineChange()<code>.
     * Note that line order in the resulting array is arbitrary
     */
    public int[] getLineNumbers();

    public int getFunctionCount();

    public DebuggableScript getFunction(int index);

    public DebuggableScript getParent();

}
