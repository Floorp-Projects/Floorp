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
 * this class has a dual nature. Generated scripts will have a null
 * 'script' field and will override 'exec' and 'call'. Scripts created
 * using the JavaScript constructor will forward requests to the
 * nonnull 'script' field.
 *
 * @since 1.3
 * @author Norris Boyd
 */

public class NativeScript extends NativeFunction implements Script {

    static void init(Context cx, Scriptable scope, boolean sealed) {
        NativeScript obj = new NativeScript();
        obj.scopeInit(cx, scope, sealed);
    }

    public NativeScript() {
    }

    private void scopeInit(Context cx, Scriptable scope, boolean sealed) {
        prototypeIdShift = getMaxId();
        addAsPrototype(prototypeIdShift + MAX_PROTOTYPE_ID,
                       cx, scope, sealed);
    }

    /**
     * Returns the name of this JavaScript class, "Script".
     */
    public String getClassName() {
        return "Script";
    }

    /**
     * Initialize script.
     *
     * Does nothing here, but scripts will override with code
     * to initialize contained functions, regexp literals, etc.
     */
    public void initScript(Scriptable scope) {
    }

    public int methodArity(int methodId) {
        if (0 <= prototypeIdShift) {
            switch (methodId - prototypeIdShift) {
                case Id_constructor: return 1;
                case Id_toString:    return 0;
                case Id_exec:        return 0;
                case Id_compile:     return 1;
            }
        }
        return super.methodArity(methodId);
    }

    public Object execMethod(int methodId, IdFunction f, Context cx,
                             Scriptable scope, Scriptable thisObj,
                             Object[] args)
        throws JavaScriptException
    {
        if (0 <= prototypeIdShift) {
            switch (methodId - prototypeIdShift) {
                case Id_constructor:
                    return jsConstructor(cx, scope, args);

                case Id_toString:
                    return realThis(thisObj, f, true).js_toString(cx, args);

                case Id_exec:
                    return realThis(thisObj, f, true).js_exec();

                case Id_compile:
                    return realThis(thisObj, f, false).
                        js_compile(cx, ScriptRuntime.toString(args, 0));
            }
        }

        return super.execMethod(methodId, f, cx, scope, thisObj, args);
    }

    private NativeScript realThis(Scriptable thisObj, IdFunction f,
                                  boolean readOnly)
    {
        while (!(thisObj instanceof NativeScript)) {
            thisObj = nextInstanceCheck(thisObj, f, readOnly);
        }
        return (NativeScript)thisObj;
    }

    /**
     * The Java method defining the JavaScript Script constructor.
     *
     */
    private static Object jsConstructor(Context cx, Scriptable scope,
                                        Object[] args)
    {
        String source = (args.length == 0)
                        ? ""
                        : ScriptRuntime.toString(args[0]);
        return compile(cx, scope, source);
    }

    private static Script compile(Context cx, Scriptable scope, String source)
    {
        StringReader reader = new StringReader(source);
        try {
            int[] linep = { 0 };
            String filename = Context.getSourcePositionFromStack(linep);
            if (filename == null) {
                filename = "<Script object>";
                linep[0] = 1;
            }
            return cx.compileReader(scope, reader, filename, linep[0], null);
        }
        catch (IOException e) {
            throw new RuntimeException("Unexpected IOException");
        }
    }

    private Scriptable js_compile(Context cx, String source) {
        script = compile(cx, null, source);
        return this;
    }

    private Object js_exec() throws JavaScriptException {
        throw Context.reportRuntimeError1
            ("msg.cant.call.indirect", "exec");
    }

    private Object js_toString(Context cx, Object[] args) {
        Script thisScript = script;
        if (thisScript == null) { thisScript = this; }
        Scriptable scope = getTopLevelScope(this);
        return cx.decompileScript(thisScript, scope, 0);
    }

    /*
     * Override method in NativeFunction to avoid ever returning "anonymous"
     */
    public String getFunctionName() {
        return "";
    }

    /**
     * Execute the script.
     *
     * Will be overridden by generated scripts; needed to implement Script.
     */
    public Object exec(Context cx, Scriptable scope)
        throws JavaScriptException
    {
        return script == null ? Undefined.instance : script.exec(cx, scope);
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException
    {
        return exec(cx, scope);
    }

    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
        throws JavaScriptException
    {
        throw Context.reportRuntimeError0("msg.script.is.not.constructor");
    }

    protected String getIdName(int id) {
        if (0 <= prototypeIdShift) {
            switch (id - prototypeIdShift) {
                case Id_constructor: return "constructor";
                case Id_toString:    return "toString";
                case Id_exec:        return "exec";
                case Id_compile:     return "compile";
            }
        }
        return super.getIdName(id);
    }

    protected int mapNameToId(String s) {
        if (0 <= prototypeIdShift) {
            int id = toPrototypeId(s);
            if (id != 0) {
                // Shift [1, MAX_PROTOTYPE_ID] to
                // [NativeFunction.getMaxId() + 1,
                //    NativeFunction.getMaxId() + MAX_PROTOTYPE_ID]
                return prototypeIdShift + id;
            }
        }
        return super.mapNameToId(s);
    }

// #string_id_map#

    private static int toPrototypeId(String s) {
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

    // "0 <= prototypeIdShift" serves as indicator of prototype instance
    // and as id offset to take into account ids present in each instance
    // of the base class NativeFunction.
    private int prototypeIdShift = -1;
}

