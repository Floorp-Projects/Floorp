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

import java.io.StringReader;
import java.io.IOException;

/**
 * The JavaScript Script object.
 *
 * Note that the C version of the engine uses XDR as the format used
 * by freeze and thaw. Since this depends on the internal format of
 * structures in the C runtime, we cannot duplicate it.
 *
 * Since we cannot replace 'this' as a result of the compile method,
 * will forward requests to execute to the nonnull 'script' field.
 *
 * @since 1.3
 * @author Norris Boyd
 */

class NativeScript extends NativeFunction implements Script
{
    private static final Object SCRIPT_TAG = new Object();

    static void init(Context cx, Scriptable scope, boolean sealed)
    {
        NativeScript obj = new NativeScript(null);
        obj.exportAsJSClass(MAX_PROTOTYPE_ID, scope, sealed);
    }

    private NativeScript(Script script)
    {
        this.script = script;
    }

    /**
     * Returns the name of this JavaScript class, "Script".
     */
    public String getClassName()
    {
        return "Script";
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException
    {
        if (script != null) {
            return script.exec(cx, scope);
        }
        return Undefined.instance;
    }

    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
        throws JavaScriptException
    {
        throw Context.reportRuntimeError0("msg.script.is.not.constructor");
    }

    public int getLength()
    {
        return 0;
    }

    public int getArity()
    {
        return 0;
    }

    public String getEncodedSource()
    {
        if (script instanceof NativeFunction) {
            return ((NativeFunction)script).getEncodedSource();
        }
        return super.getEncodedSource();
    }

    /**
     * @deprecated
     *
     * NativeScript implements {@link Script} and its
     * {@link Script#exec(Context cx, Scriptable scope)} method only for
     * backward compatibility.
     */
    public Object exec(Context cx, Scriptable scope)
        throws JavaScriptException
    {
        return script == null ? Undefined.instance : script.exec(cx, scope);
    }

    protected void initPrototypeId(int id)
    {
        String s;
        int arity;
        switch (id) {
          case Id_constructor: arity=1; s="constructor"; break;
          case Id_toString:    arity=0; s="toString";    break;
          case Id_exec:        arity=0; s="exec";        break;
          case Id_compile:     arity=1; s="compile";     break;
          default: throw new IllegalArgumentException(String.valueOf(id));
        }
        initPrototypeMethod(SCRIPT_TAG, id, s, arity);
    }

    public Object execIdCall(IdFunctionObject f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        if (!f.hasTag(SCRIPT_TAG)) {
            return super.execIdCall(f, cx, scope, thisObj, args);
        }
        int id = f.methodId();
        switch (id) {
          case Id_constructor: {
            String source = (args.length == 0)
                            ? ""
                            : ScriptRuntime.toString(args[0]);
            Script script = compile(cx, source);
            NativeScript nscript = new NativeScript(script);
            nscript.setParentScope(scope);
            nscript.setPrototype(getClassPrototype(scope, "Script"));
            return nscript;
          }

          case Id_toString: {
            NativeScript real = realThis(thisObj, f);
            Script realScript = real.script;
            if (realScript == null) { realScript = real; }
            return cx.decompileScript(realScript,
                                      getTopLevelScope(scope), 0);
          }

          case Id_exec: {
            throw Context.reportRuntimeError1(
                "msg.cant.call.indirect", "exec");
          }

          case Id_compile: {
            NativeScript real = realThis(thisObj, f);
            String source = ScriptRuntime.toString(args, 0);
            real.script = compile(cx, source);
            return real;
          }
        }
        throw new IllegalArgumentException(String.valueOf(id));
    }

    private static NativeScript realThis(Scriptable thisObj, IdFunctionObject f)
    {
        if (!(thisObj instanceof NativeScript))
            throw incompatibleCallError(f);
        return (NativeScript)thisObj;
    }

    private static Script compile(Context cx, String source)
    {
        int[] linep = { 0 };
        String filename = Context.getSourcePositionFromStack(linep);
        if (filename == null) {
            filename = "<Script object>";
            linep[0] = 1;
        }
        return cx.compileString(source, filename, linep[0], null);
    }

// #string_id_map#

    protected int findPrototypeId(String s)
    {
        int id;
// #generated# Last update: 2001-05-23 13:25:01 GMT+02:00
        L0: { id = 0; String X = null;
            L: switch (s.length()) {
            case 4: X="exec";id=Id_exec; break L;
            case 7: X="compile";id=Id_compile; break L;
            case 8: X="toString";id=Id_toString; break L;
            case 11: X="constructor";id=Id_constructor; break L;
            }
            if (X!=null && X!=s && !X.equals(s)) id = 0;
        }
// #/generated#
        return id;
    }

    private static final int
        Id_constructor    = 1,
        Id_toString       = 2,
        Id_compile        = 3,
        Id_exec           = 4,
        MAX_PROTOTYPE_ID  = 4;

// #/string_id_map#

    private Script script;
}

