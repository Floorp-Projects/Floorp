/*
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
 * Patrick Beard
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
import java.lang.reflect.InvocationTargetException;

/**
 * Load generated classes.
 *
 * @author Norris Boyd
 */
public class DefiningClassLoader extends ClassLoader
    implements GeneratedClassLoader
{
    public DefiningClassLoader() {
        init(getClass().getClassLoader());
    }

    public DefiningClassLoader(ClassLoader parentLoader) {
        init(parentLoader);
    }

    private void init(ClassLoader parentLoader) {
        this.parentLoader = parentLoader;
        this.contextLoader = null;
        if (method_getContextClassLoader != null) {
            try {
                this.contextLoader = (ClassLoader)
                    method_getContextClassLoader.invoke(
                        Thread.currentThread(),
                        ScriptRuntime.emptyArgs);
            } catch (IllegalAccessException ex) {
            } catch (InvocationTargetException ex) {
            } catch (SecurityException ex) {
            }
            if (this.contextLoader == this.parentLoader) {
                this.contextLoader = null;
            }
        }
    }

    public Class defineClass(String name, byte[] data) {
        return super.defineClass(name, data, 0, data.length);
    }

    public void linkClass(Class cl) {
        resolveClass(cl);
    }

    public Class loadClass(String name, boolean resolve)
        throws ClassNotFoundException
    {
        Class cl = findLoadedClass(name);
        if (cl == null) {
            // First try parent class loader and if that does not work, try
            // contextLoader, but that will be null if
            // Thread.getContextClassLoader() == parentLoader
            // or on JDK 1.1 due to lack Thread.getContextClassLoader().
            // To avoid catching and rethrowing ClassNotFoundException
            // in this cases, use try/catch check only if contextLoader != null.
            if (contextLoader == null) {
                cl = loadFromParent(name);
            } else {
                try {
                    cl = loadFromParent(name);
                } catch (ClassNotFoundException ex) {
                    cl = contextLoader.loadClass(name);
                }
            }
        }
        if (resolve) {
            resolveClass(cl);
        }
        return cl;
    }

    private Class loadFromParent(String name)
        throws ClassNotFoundException
    {
        if (parentLoader != null) {
            return parentLoader.loadClass(name);
        } else {
            return findSystemClass(name);
        }
    }

    private ClassLoader parentLoader;
    private ClassLoader contextLoader;

    // We'd like to use "Thread.getContextClassLoader", but
    // that's only available on Java2.
    private static Method method_getContextClassLoader;

    static {
        try {
            // Don't use "Thread.class": that performs the lookup
            // in the class initializer, which doesn't allow us to
            // catch possible security exceptions.
            Class threadClass = Class.forName("java.lang.Thread");
            method_getContextClassLoader =
                threadClass.getDeclaredMethod("getContextClassLoader",
                                               new Class[0]);
        } catch (ClassNotFoundException e) {
        } catch (NoSuchMethodException e) {
        } catch (SecurityException e) {
        }
    }
}
