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
 * Igor Bukanov
 * Roger Lawrence
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
 *
 * The class of error objects
 *
 *  ECMA 15.11
 */
final class NativeError extends IdScriptable
{

    static void init(Context cx, Scriptable scope, boolean sealed)
    {
        NativeError obj = new NativeError();
        obj.prototypeFlag = true;
        ScriptableObject.putProperty(obj, "name", "Error");
        ScriptableObject.putProperty(obj, "message", "");
        ScriptableObject.putProperty(obj, "fileName", "");
        ScriptableObject.putProperty(obj, "lineNumber", new Integer(0));
        obj.addAsPrototype(MAX_PROTOTYPE_ID, cx, scope, sealed);
    }

    static NativeError make(Context cx, Scriptable scope, IdFunction ctorObj,
                            Object[] args)
    {
        Scriptable proto = (Scriptable)(ctorObj.get("prototype", ctorObj));

        NativeError obj = new NativeError();
        obj.setPrototype(proto);
        obj.setParentScope(scope);

        if (args.length >= 1) {
            ScriptableObject.putProperty(obj, "message",
                                         ScriptRuntime.toString(args[0]));
            if (args.length >= 2) {
                ScriptableObject.putProperty(obj, "fileName", args[1]);
                if (args.length >= 3) {
                    int line = ScriptRuntime.toInt32(args[2]);
                    ScriptableObject.putProperty(obj, "lineNumber",
                                                 new Integer(line));
                }
            }
        }
        return obj;
    }

    public int methodArity(IdFunction f)
    {
        if (prototypeFlag) {
            switch (f.methodId) {
              case Id_constructor: return 1;
              case Id_toString: return 0;
              case Id_toSource: return 0;
            }
        }
        return super.methodArity(f);
    }

    public Object execMethod(IdFunction f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        if (prototypeFlag) {
            switch (f.methodId) {
              case Id_constructor:
                return make(cx, scope, f, args);

              case Id_toString:
                return js_toString(thisObj);

              case Id_toSource:
                return js_toSource(cx, scope, thisObj);
            }
        }
        return super.execMethod(f, cx, scope, thisObj, args);
    }

    private static String js_toString(Scriptable thisObj)
    {
        return getString(thisObj, "name")+": "+getString(thisObj, "message");
    }

    private static String js_toSource(Context cx, Scriptable scope,
                                      Scriptable thisObj)
        throws JavaScriptException
    {
        // Emulation of SpiderMonkey behavior
        Object name = ScriptableObject.getProperty(thisObj, "name");
        Object message = ScriptableObject.getProperty(thisObj, "message");
        Object fileName = ScriptableObject.getProperty(thisObj, "fileName");
        Object lineNumber = ScriptableObject.getProperty(thisObj, "lineNumber");

        StringBuffer sb = new StringBuffer();
        sb.append("(new ");
        if (name == NOT_FOUND) {
            name = Undefined.instance;
        }
        sb.append(ScriptRuntime.toString(name));
        sb.append("(");
        if (message != NOT_FOUND
            || fileName != NOT_FOUND
            || lineNumber != NOT_FOUND)
        {
            if (message == NOT_FOUND) {
                message = "";
            }
            sb.append(ScriptRuntime.uneval(cx, scope, message));
            if (fileName != NOT_FOUND || lineNumber != NOT_FOUND) {
                sb.append(", ");
                if (fileName == NOT_FOUND) {
                    fileName = "";
                }
                sb.append(ScriptRuntime.uneval(cx, scope, fileName));
                if (lineNumber != NOT_FOUND) {
                    int line = ScriptRuntime.toInt32(lineNumber);
                    if (line != 0) {
                        sb.append(", ");
                        sb.append(ScriptRuntime.toString(line));
                    }
                }
            }
        }
        sb.append("))");
        return sb.toString();
    }

    public String getClassName()
    {
        return "Error";
    }

    public String toString()
    {
        return js_toString(this);
    }

    private static String getString(Scriptable obj, String id)
    {
        Object value = ScriptableObject.getProperty(obj, id);
        if (value == NOT_FOUND) return "";
        return ScriptRuntime.toString(value);
    }

    protected String getIdName(int id)
    {
        if (prototypeFlag) {
            if (id == Id_constructor) return "constructor";
            if (id == Id_toString) return "toString";
            if (id == Id_toSource) return "toSource";
        }
        return null;
    }

    protected int mapNameToId(String s)
    {
        int id;
        if (!prototypeFlag) { return 0; }

// #string_id_map#
// #generated# Last update: 2004-03-17 13:35:15 CET
        L0: { id = 0; String X = null; int c;
            int s_length = s.length();
            if (s_length==8) {
                c=s.charAt(3);
                if (c=='o') { X="toSource";id=Id_toSource; }
                else if (c=='t') { X="toString";id=Id_toString; }
            }
            else if (s_length==11) { X="constructor";id=Id_constructor; }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
        return id;
    }

    private static final int
        Id_constructor    = 1,
        Id_toString       = 2,
        Id_toSource       = 3,

        MAX_PROTOTYPE_ID  = 3;

// #/string_id_map#

    private boolean prototypeFlag;
}
