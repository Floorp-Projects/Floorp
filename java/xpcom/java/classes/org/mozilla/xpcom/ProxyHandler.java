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
        /*
         *if ("toString".equals(method.getName()) &&
         *   (method.getParameterTypes()).length == 0) { //it is String toString() method
         *   return "ProxyObject@{oid = "+oid+" iid = "+iid+"}";
         *}
         */
        return Utilities.callMethod(oid, method, iid, orb, args);
    }
    private long oid;
    private IID iid;
    private long orb;
}

