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

package org.mozilla.classfile;

import java.lang.reflect.Method;
import java.lang.reflect.InvocationTargetException;

import org.mozilla.javascript.GeneratedClassLoader;

/**
 * Load generated classes.
 *
 * @author Norris Boyd
 */
public class DefiningClassLoader extends ClassLoader
    implements GeneratedClassLoader
{

    public static ClassLoader getContextClassLoader() {
        try {
            if (getContextClassLoaderMethod != null) {
                return (ClassLoader) getContextClassLoaderMethod.invoke(
                                    Thread.currentThread(),
                                    new Object[0]);
            }
        } catch (IllegalAccessException e) {
            // fall through...
        } catch (InvocationTargetException e) {
            // fall through...
        }
        return DefiningClassLoader.class.getClassLoader();
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
        Class clazz = findLoadedClass(name);
        if (clazz == null) {
            ClassLoader loader = getContextClassLoader();
            if (loader != null) {
                clazz = loader.loadClass(name);
            } else {
                clazz = findSystemClass(name);
            }
        }
        if (resolve)
            resolveClass(clazz);
        return clazz;
    }

    private static Method getContextClassLoaderMethod;
    static {
        try {
            // Don't use "Thread.class": that performs the lookup
            // in the class initializer, which doesn't allow us to
            // catch possible security exceptions.
            Class threadClass = Class.forName("java.lang.Thread");
            // We'd like to use "getContextClassLoader", but
            // that's only available on Java2.
            getContextClassLoaderMethod =
                threadClass.getDeclaredMethod("getContextClassLoader",
                                               new Class[0]);
        } catch (ClassNotFoundException e) {
            // ignore exceptions; we'll use Class.forName instead.
        } catch (NoSuchMethodException e) {
            // ignore exceptions; we'll use Class.forName instead.
        } catch (SecurityException e) {
            // ignore exceptions; we'll use Class.forName instead.
        }
    }

}
