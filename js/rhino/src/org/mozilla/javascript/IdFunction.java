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

public class IdFunction extends BaseFunction
{
    public IdFunction(IdFunctionMaster master, Object tag, int id,
                      String name, int arity, Scriptable scope)
    {
        this.functionName = name;
        this.master = master;
        this.tag = tag;
        this.methodId = id;
        this.arity = arity;
        setParentScope(scope);
    }

    public final boolean hasTag(Object tag)
    {
        return this.tag == tag;
    }

    public final int methodId()
    {
        return methodId;
    }

    public final void markAsConstructor(Scriptable prototypeProperty)
    {
        useCallAsConstructor = true;
        setImmunePrototypeProperty(prototypeProperty);
    }

    public final void exportAsScopeProperty(boolean seal)
    {
        exportAsScopeProperty(ScriptableObject.DONTENUM, seal);
    }

    public void exportAsScopeProperty(int attributes, boolean seal)
    {
        if (seal) { sealObject(); }
        ScriptableObject.defineProperty(getParentScope(), functionName, this,
                                        attributes);
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

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException
    {
        return master.execMethod(this, cx, scope, thisObj, args);
    }

    public Scriptable createObject(Context cx, Scriptable scope)
    {
        if (useCallAsConstructor) {
            return null;
        }
        // Throw error if not explicitly coded to be used as constructor,
        // to satisfy ECMAScript standard (see bugzilla 202019).
        // To follow current (2003-05-01) SpiderMonkey behavior, change it to:
        // return super.createObject(cx, scope);
        throw ScriptRuntime.typeError1("msg.not.ctor", functionName);
    }

    String decompile(int indent, int flags)
    {
        StringBuffer sb = new StringBuffer();
        boolean justbody = (0 != (flags & Decompiler.ONLY_BODY_FLAG));
        if (!justbody) {
            sb.append("function ");
            sb.append(getFunctionName());
            sb.append("() { ");
        }
        sb.append("[native code for ");
        if (master instanceof Scriptable) {
            Scriptable smaster = (Scriptable)master;
            sb.append(smaster.getClassName());
            sb.append('.');
        }
        sb.append(getFunctionName());
        sb.append(", arity=");
        sb.append(getArity());
        sb.append(justbody ? "]\n" : "] }\n");
        return sb.toString();
    }

    public int getArity()
    {
        return arity;
    }

    public int getLength() { return getArity(); }

    public final RuntimeException unknown()
    {
        // It is program error to call id-like methods for unknown or
        // non-function id
        return new RuntimeException("BAD FUNCTION ID="+methodId+" MASTER="+master);
    }

    private final IdFunctionMaster master;
    private final Object tag;
    private final int methodId;
    private int arity;
    private boolean useCallAsConstructor;
}
