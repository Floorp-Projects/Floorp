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
public final class LazilyLoadedCtor {

    public LazilyLoadedCtor(ScriptableObject scope,
                     String ctorName, String className, boolean sealed)
    {

        this.className = className;
        this.ctorName = ctorName;
        this.sealed = sealed;

        if (getter == null) {
            Method[] methods = FunctionObject.getMethodList(getClass());
            getter = FunctionObject.findSingleMethod(methods, "getProperty");
            setter = FunctionObject.findSingleMethod(methods, "setProperty");
        }

        scope.defineProperty(ctorName, this, getter, setter,
                             ScriptableObject.DONTENUM);
    }

    public Object getProperty(ScriptableObject obj) {
        synchronized (obj) {
            if (!isReplaced) {
                boolean removeOnError = false;
                Class cl = Kit.classOrNull(className);
                if (cl == null) {
                    removeOnError = true;
                } else {
                    try {
                        ScriptableObject.defineClass(obj, cl, sealed);
                        isReplaced = true;
                    } catch (InvocationTargetException ex) {
                        Throwable target = ex.getTargetException();
                        if (target instanceof RuntimeException) {
                            throw (RuntimeException)target;
                        }
                        removeOnError = true;
                    } catch (RhinoException ex) {
                        removeOnError = true;
                    } catch (InstantiationException ex) {
                        removeOnError = true;
                    } catch (IllegalAccessException ex) {
                        removeOnError = true;
                    } catch (SecurityException ex) {
                        // Treat security exceptions as absence of object.
                        // They can be due to the following reasons:
                        //  java.lang.RuntimePermission createClassLoader
                        removeOnError = true;
                    } catch (LinkageError ex) {
                        // No dependant classes
                        removeOnError = true;
                    }
                }
                if (removeOnError) {
                    obj.delete(ctorName);
                    return Scriptable.NOT_FOUND;
                }
            }
        }
        // Get just added object
        return obj.get(ctorName, obj);
    }

    public Object setProperty(ScriptableObject obj, Object val) {
        synchronized (obj) {
            isReplaced = true;
            return val;
        }
    }

    private static Method getter, setter;

    private String ctorName;
    private String className;
    private boolean sealed;
    private boolean isReplaced;
}
