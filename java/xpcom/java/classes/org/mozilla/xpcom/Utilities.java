/* -*- Mode: java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Kushnirskiy <idk@eng.sun.com>
 */
package org.mozilla.xpcom;

import java.lang.reflect.*;

public class Utilities {
    static Class objectArrayClass = (new Object[1]).getClass();
    static Object callMethodByIndex(Object obj, IID iid, int mid, Object[] args) {
        Debug.log("--[java]org.mozilla.xpcom.Utilities.callMethodByIndex "+args.length+" "+mid);
        Object retObject = null;
        for (int i = 0; i < args.length; i++) {
            Debug.log("--[java]callMethodByIndex args["+i+"] = "+args[i]);
        }
        Method method = InterfaceRegistry.getMethodByIndex(mid,iid);
        Debug.log("--[java] org.mozilla.xpcom.Utilities.callMethodByIndex method "+method);
        try {
            
            if (method != null) {
                for (int i = 0 ; i < args.length; i++) { 
                    /* this is hack. at the time we are doing holders for [out] interfaces 
                       we might do not know the expected type and we are producing Object[] insted of
                       nsISupports[] for example. Here we are taking care about it.
                       If args[i] is Object[] of size 1 we are checking with expected type from method.
                       In case it is not expeceted type we are creating object[] of expected type.
                    */  

                    if (objectArrayClass.equals(args[i].getClass()) 
                        && ((Object[])args[i]).length == 1) {
                        Class[] parameterTypes = method.getParameterTypes();
                        if (!objectArrayClass.equals(parameterTypes[i])
                            && parameterTypes[i].isArray()) {
                            Class componentType = parameterTypes[i].getComponentType();
                            args[i] = java.lang.reflect.Array.newInstance(componentType,1);
                        }
                    }
                }
                retObject = method.invoke(obj,args); 
                Debug.log("--[java] Utilities.callMethodByIndex: retObject = " + retObject);
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
        Debug.log("--[java] Utilities.callMethodByIndex method finished"+method);
        return retObject;
    }

    static Object callMethod(long oid, Method method, IID iid, long orb , Object[] args) {
        Debug.log("--[java]Utilities.callMethod "+method);
        int mid = InterfaceRegistry.getIndexByMethod(method, iid);
        if (mid < 0) {
            Debug.log("--[java]Utilities.callMethod we do not have implementation for "+method);
            return null;
        }
        Debug.log("--[java]Utilities.callMethod "+mid);
        return callMethodByIndex(oid,mid,iid.getString(), orb, args);
    }

    private static native  Object callMethodByIndex(long oid, int index, String iid, long orb, Object[] args);
    static native String[] getInterfaceMethodNames(String iid);
    static {
        System.loadLibrary("bcjavastubs");
    }
}









