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
    public static Object js_Script(Context cx, Object[] args,
                                   Function ctorObj, boolean inNewExpr)
    {
        String source = args.length == 0
                        ? ""
                        : ScriptRuntime.toString(args[0]);
        return compile(ctorObj, source);
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

    public Scriptable js_compile(String source) {
        script = compile(null, source);
        return this;
    }

    public Object js_exec() throws JavaScriptException {
        Context cx = Context.getContext();
        Scriptable scope = getTopLevelScope(getParentScope());
        return exec(cx, scope);
    }

    public static Object js_toString(Context cx, Scriptable thisObj,
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
    public String js_getName() {
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
        String message = Context.getMessage("msg.script.is.not.constructor", null);
        throw Context.reportRuntimeError(message);
    }

    private Script script;
}

