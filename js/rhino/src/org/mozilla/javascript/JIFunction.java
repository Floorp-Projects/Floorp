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

package org.mozilla.javascript;

/**
 * JIFunction or Java Implemented function is a class to simplify implementation
 * of JS functions in native code.
 */

public abstract class JIFunction extends BaseFunction
{
    public JIFunction() { }

    public JIFunction(String name, int arity)
    {
        initNameArity(name, arity);
    }

    protected final void initNameArity(String name, int arity)
    {
        this.functionName = name;
        this.arity = arity;
    }

    public final void defineAsProperty(Scriptable scope)
    {
        defineAsProperty(scope, ScriptableObject.DONTENUM, false);
    }

    public final void defineAsProperty(Scriptable scope, int attributes)
    {
        defineAsProperty(scope, attributes, false);
    }

    public final void defineAsProperty(Scriptable scope, int attributes,
                                       boolean sealed)
    {
        setParentScope(scope);
        if (sealed) { sealObject(); }
        ScriptableObject.defineProperty(scope, functionName, this, attributes);
    }

    public Scriptable getPrototype()
    {
        // Lazy initialization of prototype: for native functions this
        // may not be called at all
        Scriptable proto = super.getPrototype();
        if (proto == null) {
            proto = getFunctionPrototype(getParentScope());
            setPrototype(proto);
        }
        return proto;
    }

    public abstract Object call(Context cx, Scriptable scope,
                                Scriptable thisObj, Object[] args)
        throws JavaScriptException;

    public int getArity()
    {
        return arity;
    }

    public int getLength()
    {
        return getArity();
    }

    private int arity;
}
