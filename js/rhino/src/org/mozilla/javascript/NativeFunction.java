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
 * Roger Lawrence
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
 * This class implements the Function native object.
 * See ECMA 15.3.
 * @author Norris Boyd
 */
public class NativeFunction extends BaseFunction
{

    public final void initScriptFunction(Context cx, String functionName,
                                         String[] argNames, int argCount)
    {
        if (!(argNames != null
              && 0 <= argCount && argCount <= argNames.length))
        {
            throw new IllegalArgumentException();
        }

        if (!(this.argNames == null)) {
            // Initialization can only be done once
            throw new IllegalStateException();
        }

        this.functionName = functionName;
        this.argNames = argNames;
        this.argCount = (short)argCount;
        this.version = (short)cx.getLanguageVersion();
    }

    /**
     * @param cx Current context
     *
     * @param indent How much to indent the decompiled result
     *
     * @param justbody Whether the decompilation should omit the
     * function header and trailing brace.
     */
    public final String decompile(Context cx, int indent, boolean justbody)
    {
        Object encodedSource = getEncodedSource();
        if (encodedSource == null) {
            return super.decompile(cx, indent, justbody);
        } else {
            final int INDENT_GAP = 4;
            final int CASE_GAP = 2; // less how much for case labels
            return Decompiler.decompile(encodedSource, justbody,
                                        indent, INDENT_GAP, CASE_GAP);
        }
    }

    public int getLength()
    {
        Context cx = Context.getContext();
        if (cx != null && cx.getLanguageVersion() != Context.VERSION_1_2)
            return argCount;
        NativeCall activation = getActivation(cx);
        if (activation == null)
            return argCount;
        return activation.getOriginalArguments().length;
    }

    public int getArity()
    {
        return argCount;
    }

    /**
     * @deprecated Use {@link BaseFunction#getFunctionName()} instead.
     * For backwards compatibility keep an old method name used by
     * Batik and possibly others.
     */
    public String jsGet_name()
    {
        return getFunctionName();
    }

    /**
     * Get encoded source string.
     */
    public Object getEncodedSource()
    {
        return null;
    }

    /**
     * The "argsNames" array has the following information:
     * argNames[0] through argNames[argCount - 1]: the names of the parameters
     * argNames[argCount] through argNames[args.length-1]: the names of the
     * variables declared in var statements
     */
    protected String[] argNames;
    protected short argCount;

    protected short version;
}

