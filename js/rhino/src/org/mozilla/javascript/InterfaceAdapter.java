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

package org.mozilla.javascript;

import java.lang.reflect.Method;

/**
 * Adapter to use JS function as implementation of Java interfaces with
 * single method or multiple methods with the same signature.
 */
public class InterfaceAdapter
{
    private final Object proxyHelper;
    private boolean[] argsToConvert;

    /**
     * Make glue object implementing interface cl that will
     * call the supplied JS function when called.
     * Only interfaces were all methods has the same signature is supported.
     *
     * @return The glue object or null if <tt>cl</tt> is not interface or
     *         has methods with different signatures.
     */
    static Object create(Context cx, Class cl, Callable f)
    {
        Scriptable topScope = ScriptRuntime.getTopCallScope(cx);
        ClassCache cache = ClassCache.get(topScope);
        InterfaceAdapter adapter;
        adapter = (InterfaceAdapter)cache.getInterfaceAdapter(cl);
        ContextFactory cf = cx.getFactory();
        if (adapter == null) {
            if (!cl.isInterface())
                return null;
            Method[] methods = cl.getMethods();
            if (methods.length == 0) { return null; }
            Class returnType = methods[0].getReturnType();
            Class[] argTypes = methods[0].getParameterTypes();
            // check that the rest of methods has the same signature
            for (int i = 1; i != methods.length; ++i) {
                if (returnType != methods[i].getReturnType()) { return null; }
                Class[] types2 = methods[i].getParameterTypes();
                if (types2.length != argTypes.length) { return null; }
                for (int j = 0; j != argTypes.length; ++j) {
                    if (types2[j] != argTypes[j]) { return null; }
                }
            }

            adapter = new InterfaceAdapter(cf, cl, argTypes);
            cache.cacheInterfaceAdapter(cl, adapter);
        }
        return VMBridge.instance.newInterfaceProxy(
            adapter.proxyHelper, cf, adapter, f, topScope);
    }

    private InterfaceAdapter(ContextFactory cf, Class cl, Class[] argTypes)
    {
        this.proxyHelper
            = VMBridge.instance.getInterfaceProxyHelper(
                cf, new Class[] { cl });
        for (int i = 0; i != argTypes.length; ++i) {
            if (!ScriptRuntime.isRhinoRuntimeType(argTypes[i])) {
                if (argsToConvert == null) {
                    argsToConvert = new boolean[argTypes.length];
                }
                argsToConvert[i] = true;
            }
        }
    }

    public Object invoke(ContextFactory cf,
                         final Object target,
                         final Scriptable topScope,
                         final Method method,
                         final Object[] args)
    {
        ContextAction action = new ContextAction() {
                public Object run(Context cx)
                {
                    return invokeImpl(cx, target, topScope, method, args);
                }
            };
        return cf.call(action);
    }

    Object invokeImpl(Context cx,
                      Object target,
                      Scriptable topScope,
                      Method method,
                      Object[] args)
    {
        Callable callable = (Callable)target;
        int N = (args == null) ? 0 : args.length;
        Object[] jsargs = new Object[N + 1];
        if (N != 0) {
            System.arraycopy(args, 0, jsargs, 0, N);
        }
        jsargs[N] = method.getName();
        if (argsToConvert != null) {
            WrapFactory wf = cx.getWrapFactory();
            for (int i = 0; i != N; ++i) {
                if (argsToConvert[i]) {
                    jsargs[i] = wf.wrap(cx, topScope, jsargs[i], null);
                }
            }
        }

        Scriptable thisObj = topScope;
        Object result = callable.call(cx, topScope, thisObj, jsargs);
        Class javaResultType = method.getReturnType();
        if (javaResultType == Void.TYPE) {
            result = null;
        } else {
            result = Context.jsToJava(result, javaResultType);
        }
        return result;
    }
}
