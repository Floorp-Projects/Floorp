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
 * This class describes the support needed to implement security.
 * <p>
 * Three main pieces of functionality are required to implement
 * security for JavaScript. First, it must be possible to define
 * classes with an associated security domain. (This security
 * domain may be any object incorporating notion of access
 * restrictions that has meaning to an embedding; for a client-side
 * JavaScript embedding this would typically be
 * java.security.ProtectionDomain or similar object depending on an
 * origin URL and/or a digital certificate.)
 * Next it must be possible to get a security domain object that
 * allows a particular action only if all security domains
 * associated with code on the current Java stack allows it. And
 * finally, it must be possible to execute script code with
 * associated security domain injected into Java stack.
 * <p>
 * These three pieces of functionality are encapsulated in the
 * SecurityController class.
 *
 * @see org.mozilla.javascript.Context#setSecurityController(SecurityController)
 * @see java.lang.ClassLoader
 * @since 1.5 Release 4
 */
public abstract class SecurityController
{
    private static SecurityController global;

// The method must NOT be public or protected
    static SecurityController global()
    {
        return global;
    }

    /**
     * Check if global {@link SecurityController} was already installed.
     * @see #initGlobal(SecurityController controller)
     */
    public static boolean hasGlobal()
    {
        return global != null;
    }

    /**
     * Initialize global controller that will be used for all
     * security-related operations. The global controller takes precedence
     * over already installed {@link Context}-specific controllers and cause
     * any subsequent call to
     * {@link Context#setSecurityController(SecurityController)}
     * to throw an exception.
     * <p>
     * The method can only be called once.
     *
     * @see #hasGlobal()
     */
    public static void initGlobal(SecurityController controller)
    {
        if (controller == null) throw new IllegalArgumentException();
        if (global != null) {
            throw new SecurityException("Cannot overwrite already installed global SecurityController");
        }
        global = controller;
    }

    /**
     * Get class loader-like object that can be used
     * to define classes with the given security context.
     * @param parentLoader parent class loader to delegate search for classes
     *        not defined by the class loader itself
     * @param securityDomain some object specifying the security
     *        context of the code that is defined by the returned class loader.
     */
    public abstract GeneratedClassLoader createClassLoader(
        ClassLoader parentLoader, Object securityDomain);

    /**
     * Create {@link GeneratedClassLoader} with restrictions imposed by
     * staticDomain and all current stack frames.
     * The method uses the SecurityController instance associated with the
     * current {@link Context} to construct proper dynamic domain and create
     * corresponding class loader.
     * <par>
     * If no SecurityController is associated with the current {@link Context} ,
     * the method calls {@link Context#createClassLoader(ClassLoader parent)}.
     *
     * @param parent parent class loader. If null,
     *        {@link Context#getApplicationClassLoader()} will be used.
     * @param staticDomain static security domain.
     */
    public static GeneratedClassLoader createLoader(
        ClassLoader parent, Object staticDomain)
    {
        Context cx = Context.getContext();
        if (parent == null) {
            parent = cx.getApplicationClassLoader();
        }
        SecurityController sc = cx.getSecurityController();
        GeneratedClassLoader loader;
        if (sc == null) {
            loader = cx.createClassLoader(parent);
        } else {
            Object dynamicDomain = sc.getDynamicSecurityDomain(staticDomain);
            loader = sc.createClassLoader(parent, dynamicDomain);
        }
        return loader;
    }

    /**
     * Get dynamic security domain that allows an action only if it is allowed
     * by the current Java stack and <i>securityDomain</i>. If
     * <i>securityDomain</i> is null, return domain representing permissions
     * allowed by the current stack.
     */
    public abstract Object getDynamicSecurityDomain(Object securityDomain);

    /**
     * Call {@link
     * Callable#call(Context cx, Scriptable scope, Scriptable thisObj,
     *               Object[] args)}
     * of <i>callable</i> under restricted security domain where an action is
     * allowed only if it is allowed according to the Java stack on the
     * moment of the <i>execWithDomain</i> call and <i>securityDomain</i>.
     * Any call to {@link #getDynamicSecurityDomain(Object)} during
     * execution of <tt>callable.call(cx, scope, thisObj, args)</tt>
     * should return a domain incorporate restrictions imposed by
     * <i>securityDomain</i> and Java stack on the moment of callWithDomain
     * invocation.
     * <p>
     * The method should always be overridden, it is not declared abstract
     * for compatibility reasons.
     */
    public Object callWithDomain(Object securityDomain, Context cx,
                                 final Callable callable, Scriptable scope,
                                 final Scriptable thisObj, final Object[] args)
        throws JavaScriptException
    {
        return execWithDomain(cx, scope, new Script()
        {
            public Object exec(Context cx, Scriptable scope)
                throws JavaScriptException
            {
                return callable.call(cx, scope, thisObj, args);
            }

        }, securityDomain);
    }

    /**
     * @deprecated The application should not override this method and instead
     * override
     * {@link #callWithDomain(Object securityDomain, Context cx, Callable callable, Scriptable scope, Scriptable thisObj, Object[] args)}.
     */
    public Object execWithDomain(Context cx, Scriptable scope,
                                 Script script, Object securityDomain)
        throws JavaScriptException
    {
        throw new IllegalStateException("callWithDomain should be overridden");
    }


}
