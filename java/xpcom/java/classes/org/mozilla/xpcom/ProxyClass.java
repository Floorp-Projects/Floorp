/*
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
import java.util.*;

public class ProxyClass { //nb it should not be public
    public ProxyClass(IID _iid, Method[] _methods) { //nb it should not be public
        iid = _iid;
        methods = _methods;
        if (classes == null) {
            classes = new Hashtable();
        }
        classes.put(iid, this);
    }
    Method getMethodByIndex(int mid) { //first method has index equal to 'offset'
        System.out.println("--[java]ProxyClass.GetMehodByIndex "+mid);
        Method result = null;
        try {
            result = methods[mid-offset];
        } catch (Exception e) {
            e.printStackTrace();
        }
        return result;
    }
    int getIndexByMethod(Method method) { 
        int result = 0;
        if (method == null
            ||methods == null) {
            return result;
        }
        for (int i = 0; i < methods.length; i++) {
            if (method.equals(methods[i])) {
                result = i + offset;
                break;
            }
        }
        return result;
    }

    static ProxyClass getProxyClass(IID iid) {
        ProxyClass result = null;
        Object obj =  null;
        if (classes != null) {
            obj = classes.get(iid);
            if (obj != null
                && (obj instanceof ProxyClass)) {
                result = (ProxyClass)obj;
            }
        }
        return result;
    }
    private IID iid;
    private Method[] methods;
    private final int offset = 0; //from xpcom
    static Hashtable classes = null;
}






