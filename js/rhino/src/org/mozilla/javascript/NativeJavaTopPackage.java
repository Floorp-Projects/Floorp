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
 * Frank Mitchell
 * Mike Shaver
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

import java.lang.reflect.*;

/**
 * This class reflects Java packages into the JavaScript environment.  We
 * lazily reflect classes and subpackages, and use a caching/sharing
 * system to ensure that members reflected into one JavaPackage appear
 * in all other references to the same package (as with Packages.java.lang
 * and java.lang).
 *
 * @author Mike Shaver
 * @see NativeJavaArray
 * @see NativeJavaObject
 * @see NativeJavaClass
 */

public class NativeJavaTopPackage
    extends NativeJavaPackage implements Function, IdFunctionMaster
{

    // we know these are packages so we can skip the class check
    // note that this is ok even if the package isn't present.
    private static final String commonPackages = ""
                                                 +"java.lang;"
                                                 +"java.lang.reflect"
                                                 +"java.io;"
                                                 +"java.math;"
                                                 +"java.net;"
                                                 +"java.util;"
                                                 +"java.util.zip;"
                                                 +"java.text;"
                                                 +"java.text.resources;"
                                                 +"java.applet;"
                                                 +"javax.swing;"
                                                 ;

    NativeJavaTopPackage(ClassLoader loader)
    {
        super(true, "", loader);
    }

    public Object call(Context cx, Scriptable scope, Scriptable thisObj,
                       Object[] args)
        throws JavaScriptException
    {
        return construct(cx, scope, args);
    }

    public Scriptable construct(Context cx, Scriptable scope, Object[] args)
        throws JavaScriptException
    {
        ClassLoader loader = null;
        if (args.length != 0) {
            Object arg = args[0];
            if (arg instanceof Wrapper) {
                arg = ((Wrapper)arg).unwrap();
            }
            if (arg instanceof ClassLoader) {
                loader = (ClassLoader)arg;
            }
        }
        if (loader == null) {
            Context.reportRuntimeError0("msg.not.classloader");
            return null;
        }
        return new NativeJavaPackage(true, "", loader);
    }

    public static void init(Context cx, Scriptable scope, boolean sealed)
    {
        ClassLoader loader = cx.getApplicationClassLoader();
        final NativeJavaTopPackage top = new NativeJavaTopPackage(loader);
        top.setPrototype(getObjectPrototype(scope));
        top.setParentScope(scope);

        String[] names = Kit.semicolonSplit(commonPackages);
        for (int i = 0; i != names.length; ++i) {
            top.forcePackage(names[i]);
        }

        // getClass implementation
        IdFunction getClass = new IdFunction(top, FTAG, Id_getClass,
                                             "getClass", 1);

        // We want to get a real alias, and not a distinct JavaPackage
        // with the same packageName, so that we share classes and top
        // that are underneath.
        NativeJavaPackage javaAlias = (NativeJavaPackage)top.get("java", top);

        // It's safe to downcast here since initStandardObjects takes
        // a ScriptableObject.
        ScriptableObject global = (ScriptableObject) scope;

        getClass.defineAsScopeProperty(global, sealed);
        global.defineProperty("Packages", top, ScriptableObject.DONTENUM);
        global.defineProperty("java", javaAlias, ScriptableObject.DONTENUM);
    }

    public Object execMethod(IdFunction f, Context cx, Scriptable scope,
                             Scriptable thisObj, Object[] args)
    {
        if (f.hasTag(FTAG)) {
            if (f.methodId == Id_getClass) {
                return js_getClass(cx, scope, args);
            }
        }
        throw f.unknown();
    }

    private Scriptable js_getClass(Context cx, Scriptable scope, Object[] args)
    {
        if (args.length > 0  && args[0] instanceof Wrapper) {
            Scriptable result = this;
            Class cl = ((Wrapper) args[0]).unwrap().getClass();
            // Evaluate the class name by getting successive properties of
            // the string to find the appropriate NativeJavaClass object
            String name = cl.getName();
            int offset = 0;
            for (;;) {
                int index = name.indexOf('.', offset);
                String propName = index == -1
                                  ? name.substring(offset)
                                  : name.substring(offset, index);
                Object prop = result.get(propName, result);
                if (!(prop instanceof Scriptable))
                    break;  // fall through to error
                result = (Scriptable) prop;
                if (index == -1)
                    return result;
                offset = index+1;
            }
        }
        throw Context.reportRuntimeError(
            Context.getMessage0("msg.not.java.obj"));
    }

    private static final Object FTAG = new Object();
    private static final int Id_getClass = 1;
}

