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
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Patrick C. Beard
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

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;

/**
 * ClassManager
 *
 * Provides a way to delegate class loading to an outer class loader
 * regardless of its actual type. For this to work, the class loader
 * must define the following public methods, which will be called
 * using reflection:
 *   <code>public Class defineClass(String name, byte[] data);</code>
 *   <code>public Class loadClass(String name, boolean resolve);</code>
 * This defines the protocol that will be used.
 */
public class ClassManager {
    public static Class defineClass(ClassLoader loader, String name, byte[] data)
    {
        try {
            Class[] types = { String.class, byte[].class };
            Method defineClass = loader.getClass().getMethod("defineClass", types);
            Object[] args = { name, data };
            return (Class) defineClass.invoke(loader, args);
        } catch (Exception ex) {
        }
        return null;
    }
    
    public static Class loadClass(ClassLoader loader, String name, boolean resolve)
    {
        try {
            Class[] types = { String.class, boolean.class };
            Method loadClass = loader.getClass().getMethod("loadClass", types);
            Object[] args = { name, new Boolean(resolve) };
            return (Class) loadClass.invoke(loader, args);
        } catch (Exception ex) {
        }
        return null;
    }
}
