/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

class ProxyHandler implements InvocationHandler {
    ProxyHandler(long _oid, IID _iid, long _orb) {
	oid = _oid;
	iid = _iid;
	orb = _orb;
    } 
    public Object invoke(Object proxy,
			 Method method,
			 Object[] args) throws Throwable {
        System.out.println("--[java]ProxyHandler.invoke "+method);
        String str = method.getName();
        if (str.equals("toString")) {
            return "ProxyObject@{oid = "+oid+" iid = "+iid+"}";
        } else if (str.equals("clone")) {
            throw new java.lang.CloneNotSupportedException();
        } else if (str.equals("finalize")) {
            finalize();
        } else if (str.equals("equals")) {
            if (args[0] instanceof ProxyHandler) {
                ProxyHandler p = (ProxyHandler)args[0];
                return new Boolean((oid == p.oid) &&
                                   iid.equals(p.iid) &&
                                   (orb == p.orb));
            }
        } else if (str.equals("hashCode")) {
            return new Integer(hashCode());
        } else {
            return Utilities.callMethod(oid, method, iid, orb, args);
        }
        return null;
    }

    private long oid;
    private IID iid;
    private long orb;
}

