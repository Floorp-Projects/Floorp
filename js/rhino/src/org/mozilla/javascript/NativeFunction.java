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

import java.lang.reflect.Method;

/**
 * This class implements the Function native object.
 * See ECMA 15.3.
 * @author Norris Boyd
 */
public class NativeFunction extends BaseFunction {

    /**
     * @param cx Current context
     *
     * @param indent How much to indent the decompiled result
     *
     * @param justbody Whether the decompilation should omit the
     * function header and trailing brace.
     */

    public String decompile(Context cx, int indent, boolean justbody) {
        Object sourcesTree = getSourcesTree();
        if (sourcesTree == null) {
            return super.decompile(cx, indent, justbody);
        } else {
            return Parser.decompile(sourcesTree, fromFunctionConstructor,
                                    version, indent, justbody);
        }
    }

    public int getLength() {
        Context cx = Context.getContext();
        if (cx != null && cx.getLanguageVersion() != Context.VERSION_1_2)
            return argCount;
        NativeCall activation = getActivation(cx);
        if (activation == null)
            return argCount;
        return activation.getOriginalArguments().length;
    }

    public int getArity() {
        return argCount;
    }

    public String getFunctionName() {
        if (fromFunctionConstructor) {
            if (version == Context.VERSION_1_2) {
                return "";
            }
        }
        return super.getFunctionName();
    }

    /**
     * @deprecated Use {@link #getFunctionName()} instead.
     * For backwards compatibility keep an old method name used by
     * Batik and possibly others.
     */
    public String jsGet_name() {
        return getFunctionName();
    }

    /**
     * Get encoded source as tree where node data encodes source of single
     * function as String. If node is String, it is leaf and its data is node
     * itself. Otherwise node is Object[] array, where array[0] holds node data
     * and array[1..array.length) holds child nodes.
     */
    protected Object getSourcesTree() {
        // The following is used only by optimizer, but is here to avoid
        // introduction of 2 additional classes there
        Class cl = getClass();
        try { 
            Method m = cl.getDeclaredMethod("getSourcesTreeImpl", 
                                            new Class[0]);
            return m.invoke(null, ScriptRuntime.emptyArgs);
        } catch (NoSuchMethodException ex) { 
            // No source implementation
            return null;
        } catch (Exception ex) {
            // Wrap the rest of exceptions including possible SecurityException
            throw WrappedException.wrapException(ex);
        }
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

    /**
     * True if this represents function constructed via Function()
     * constructor
     */
    boolean fromFunctionConstructor;
}

