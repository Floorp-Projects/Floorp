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

package org.mozilla.javascript;

import java.util.Hashtable;

/**
 * This class implements the Object native object.
 * See ECMA 15.2.
 * @author Norris Boyd
 */
public class NativeObject extends ScriptableObject {

    public static void finishInit(Scriptable scope, FunctionObject ctor,
                                  Scriptable proto)
    {
        Object obj = proto.get("valueOf", proto);
        ((FunctionObject) obj).setLength((short) 0);
    }

    public String getClassName() {
        return "Object";
    }

    public static Object js_Object(Context cx, Object[] args, Function ctorObj,
                                   boolean inNewExpr)
        throws JavaScriptException
    {
        if (!inNewExpr) {
            // FunctionObject.construct will set up parent, proto
            return ctorObj.construct(cx, ctorObj.getParentScope(), args);
        }
        if (args.length == 0 || args[0] == null ||
            args[0] == Undefined.instance)
        {
            return new NativeObject();
        }
        return ScriptRuntime.toObject(ctorObj.getParentScope(), args[0]);
    }

    public String toString() {
        Context cx = Context.getContext();
        if (cx != null)
            return js_toString(cx, this, null, null);
        else
            return "[object " + getClassName() + "]";
    }

    public static String js_toString(Context cx, Scriptable thisObj,
                                     Object[] args, Function funObj)
    {
        if (cx.getLanguageVersion() != cx.VERSION_1_2) 
            return "[object " + thisObj.getClassName() + "]";
        
        return toSource(cx, thisObj, args, funObj);
    }
    
    public static String toSource(Context cx, Scriptable thisObj,
                                  Object[] args, Function funObj)
    {
        Scriptable m = thisObj;
        
        if (cx.iterating == null)
            cx.iterating = new Hashtable(31);

        if (cx.iterating.get(m) == Boolean.TRUE) {
            return "{}";  // stop recursion
        } else {
            StringBuffer result = new StringBuffer("{");
            Object[] ids = m.getIds();

            for(int i=0; i < ids.length; i++) {
                if (i > 0)
                    result.append(", ");

                Object id = ids[i];
                String idString = ScriptRuntime.toString(id);
                Object p = (id instanceof String)
                    ? m.get((String) id, m)
                    : m.get(((Number) id).intValue(), m);
                if (p instanceof String) {
                    result.append(idString + ":\""
                        + ScriptRuntime
                          .escapeString(ScriptRuntime.toString(p))
                        + "\"");
                } else {
                    /* wrap changes to cx.iterating in a try/finally
                     * so that the reference always gets removed, and
                     * we don't leak memory.  Good place for weak
                     * references, if we had them.
                     */
                    try {
                        cx.iterating.put(m, Boolean.TRUE); // stop recursion.
                        result.append(idString + ":" + ScriptRuntime.toString(p));
                    } finally {
                        cx.iterating.remove(m);
                    }
                }
            }
            result.append("}");
            return result.toString();
        }
    }

    public static Object js_valueOf(Context cx, Scriptable thisObj,
                                    Object[] args, Function funObj)
    {
        return thisObj;
    }
}


