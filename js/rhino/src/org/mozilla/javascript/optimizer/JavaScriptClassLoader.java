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


package org.mozilla.javascript.optimizer;

import java.util.*;
import org.mozilla.classfile.ClassManager;

/**
 * Load generated classes that implement JavaScript scripts
 * or functions.
 *
 * @author Norris Boyd
 */
final class JavaScriptClassLoader extends ClassLoader {

    JavaScriptClassLoader() {
    }
    
    public Class defineClass(String name, byte data[]) {
        ClassLoader loader = getClass().getClassLoader();
        if (loader != null) {
            Class clazz = ClassManager.defineClass(loader, name, data);
            if (clazz != null)
                return clazz;
        }
        return super.defineClass(name, data, 0, data.length);
    }

    protected Class loadClass(String name, boolean resolve)
        throws ClassNotFoundException
    {
        Class clazz;
        ClassLoader loader = getClass().getClassLoader();
        if (loader != null) {
            clazz = ClassManager.loadClass(loader, name, resolve);
            if (clazz != null)
                return clazz;
        }
        clazz = findLoadedClass(name);
        if (clazz == null)
            clazz = findSystemClass(name);
        if (resolve)
            resolveClass(clazz);
        return clazz;
    }
}
