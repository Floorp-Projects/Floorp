/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
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

    public NativeScript() {
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

    /**
     * The Java method defining the JavaScript Script constructor.
     *
     */
    public static Object jsConstructor(Context cx, Object[] args,
                                       Function ctorObj, boolean inNewExpr)
    {
        String source = args.length == 0
                        ? ""
                        : ScriptRuntime.toString(args[0]);
        Scriptable scope = cx.ctorScope;
        if (scope == null)
            scope = ctorObj;
        return compile(scope, source);
    }

    public static Script compile(Scriptable scope, String source) {
        Context cx = Context.getContext();
        StringReader reader = new StringReader(source);
        try {
            int[] linep = { 0 };
            String filename = Context.getSourcePositionFromStack(linep);
            if (filename == null) {
                filename = "<Script object>";
                linep[0] = 1;
            }
            Object securityDomain = 
                cx.getSecurityDomainForStackDepth(5);
            return cx.compileReader(scope, reader, filename, linep[0], 
                                    securityDomain);
        }
        catch (IOException e) {
            throw new RuntimeException("Unexpected IOException");
        }
    }

    public Scriptable jsFunction_compile(String source) {
        script = compile(null, source);
        return this;
    }

    public Object jsFunction_exec() throws JavaScriptException {
        throw Context.reportRuntimeError1
            ("msg.cant.call.indirect", "exec");
    }

    public static Object jsFunction_toString(Context cx, Scriptable thisObj,
                                             Object[] args, Function funObj)
    {
        Script thisScript = ((NativeScript) thisObj).script;
        if (thisScript == null)
            thisScript = (Script) thisObj;
        Scriptable scope = getTopLevelScope(thisObj);
        return cx.decompileScript(thisScript, scope, 0);
    }
    
    /*
     * Override method in NativeFunction to avoid ever returning "anonymous"
     */
    public String jsGet_name() {
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

    private Script script;
}

