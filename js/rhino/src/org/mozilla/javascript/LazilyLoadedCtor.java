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
 * Norris Boyd
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
 * Avoid loading classes unless they are used.
 *
 * <p> This improves startup time and average memory usage.
 */
class LazilyLoadedCtor {

    LazilyLoadedCtor(ScriptableObject scope, String ctorName,
                     String className, int attributes)
        throws PropertyException
    {
        this.className = className;
        this.ctorName = ctorName;
        Class cl = getClass();
        Method[] getter = FunctionObject.findMethods(cl, "getProperty");
        Method[] setter = FunctionObject.findMethods(cl, "setProperty");
        scope.defineProperty(this.ctorName, this, getter[0], setter[0],
                             attributes);
    }

    public Object getProperty(ScriptableObject obj) {
        try {
            synchronized (obj) {
                if (!isReplaced)
                    ScriptableObject.defineClass(obj, Class.forName(className));
                isReplaced = true;
            }
        }
        catch (ClassNotFoundException e) {
            throw WrappedException.wrapException(e);
        }
        catch (InstantiationException e) {
            throw WrappedException.wrapException(e);
        }
        catch (IllegalAccessException e) {
            throw WrappedException.wrapException(e);
        }
        catch (InvocationTargetException e) {
            throw WrappedException.wrapException(e);
        }
        catch (ClassDefinitionException e) {
            throw WrappedException.wrapException(e);
        }
        catch (PropertyException e) {
            throw WrappedException.wrapException(e);
        }
        return obj.get(ctorName, obj);
    }

    public Object setProperty(ScriptableObject obj, Object val) {
        synchronized (obj) {
            isReplaced = true;
            return val;
        }
    }

    private String ctorName;
    private String className;
    private boolean isReplaced;
}
