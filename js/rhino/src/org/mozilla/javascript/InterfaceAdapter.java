/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Igor Bukanov
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above. If
 * you wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replacing
 * them with the notice and other provisions required by the GPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.javascript;

import java.lang.reflect.Method;

/**
 * Adapter to use JS function as implementation of Java interfaces with
 * single method or multiple methods with the same signature.
 */
public class InterfaceAdapter
{
    private final Object proxyHelper;

    /**
     * Make glue object implementing interface cl that will
     * call the supplied JS function when called.
     * Only interfaces were all methods have the same signature is supported.
     *
     * @return The glue object or null if <tt>cl</tt> is not interface or
     *         has methods with different signatures.
     */
    static Object create(Context cx, Class cl, Callable function)
    {
        if (!cl.isInterface()) throw new IllegalArgumentException();

        Scriptable topScope = ScriptRuntime.getTopCallScope(cx);
        ClassCache cache = ClassCache.get(topScope);
        InterfaceAdapter adapter;
        adapter = (InterfaceAdapter)cache.getInterfaceAdapter(cl);
        ContextFactory cf = cx.getFactory();
        if (adapter == null) {
            Method[] methods = cl.getMethods();
            if (methods.length == 0) {
                throw Context.reportRuntimeError2(
                    "msg.no.empty.interface.conversion",
                    String.valueOf(function),
                    cl.getClass().getName());
            }
            boolean canCallFunction = false;
          canCallFunctionChecks: {
                Class[] argTypes = methods[0].getParameterTypes();
                // check that the rest of methods has the same signature
                for (int i = 1; i != methods.length; ++i) {
                    Class[] types2 = methods[i].getParameterTypes();
                    if (types2.length != argTypes.length) {
                        break canCallFunctionChecks;
                    }
                    for (int j = 0; j != argTypes.length; ++j) {
                        if (types2[j] != argTypes[j]) {
                            break canCallFunctionChecks;
                        }
                    }
                }
                canCallFunction= true;
            }
            if (!canCallFunction) {
                throw Context.reportRuntimeError2(
                    "msg.no.function.interface.conversion",
                    String.valueOf(function),
                    cl.getClass().getName());
            }
            adapter = new InterfaceAdapter(cf, cl);
            cache.cacheInterfaceAdapter(cl, adapter);
        }
        return VMBridge.instance.newInterfaceProxy(
            adapter.proxyHelper, cf, adapter, function, topScope);
    }

    private InterfaceAdapter(ContextFactory cf, Class cl)
    {
        this.proxyHelper
            = VMBridge.instance.getInterfaceProxyHelper(
                cf, new Class[] { cl });
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
        int N = (args == null) ? 0 : args.length;

        Callable function = (Callable)target;
        Scriptable thisObj = topScope;
        Object[] jsargs = new Object[N + 1];
        jsargs[N] = method.getName();
        if (N != 0) {
            WrapFactory wf = cx.getWrapFactory();
            for (int i = 0; i != N; ++i) {
                jsargs[i] = wf.wrap(cx, topScope, args[i], null);
            }
        }

        Object result = function.call(cx, topScope, thisObj, jsargs);
        Class javaResultType = method.getReturnType();
        if (javaResultType == Void.TYPE) {
            result = null;
        } else {
            result = Context.jsToJava(result, javaResultType);
        }
        return result;
    }
}
