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

import java.util.*;
import java.lang.reflect.*;
import java.lang.ref.*;

class ProxyKey {
    ProxyKey(long _oid, IID _iid) {
	oid = new Long(_oid);
	iid = _iid;
    }
    public boolean equals(Object obj) {
	if (! (obj instanceof ProxyKey)) { 
	    return false;
	}
	return (oid.equals(((ProxyKey)obj).oid) && iid.equals(((ProxyKey)obj).iid));
    }
    public int hashCode() {
	return oid.hashCode();
    }
    public String toString() {
	return "org.mozilla.xpcom.ProxyFactory.ProxyKey "+oid+" "+iid;
    }
    Long oid;
    IID iid;
}

public class ProxyFactory {
    public static void registerInterfaceForIID(Class inter, IID iid) {
        System.out.println("--[java] ProxyFactory.registerInterfaceForIID "+iid);
        if (interfaces == null) {
            interfaces = new Hashtable();
        }
        interfaces.put(iid, inter);  //nb who is gonna remove object from cache?
    }
    public static Class getInterface(IID iid) {
        System.out.println("--[java] ProxyFactory.getInterface "+iid);
        Object obj = null;
        if (interfaces != null) {
            obj = interfaces.get(iid);
            if (obj == null) {
                System.out.println("--[java] ProxyFactory.getInterface interface== null");
                return null;
            }
        }
        if (!(obj instanceof Class)) {
            System.out.println("--[java] ProxyFactory.getInterface !(obj instanceof Class"+obj);
            return null;
        }
        return (Class)obj;
    }

    public static Object getProxy(long oid, IID iid, long orb) {
        try {
        System.out.println("--[java] ProxyFactory.getProxy "+iid);
        ProxyKey key = new ProxyKey(oid, iid);
        Object obj = null;
        Object result = null;
        if (proxies != null) {
            obj = proxies.get(key);
            if (obj != null 
                && (obj instanceof Reference)) {
                result = ((Reference)obj).get();
            }
        } else {
            proxies = new Hashtable();
        }
        if (result == null) {
            Class inter = getInterface(iid);
            if (inter == null) {
                System.out.println("--[java] ProxyFactory.getProxy we did not find interface for iid="+iid+"returing null");
                return null;
            }
            InvocationHandler handler = new ProxyHandler(oid, iid, orb);
            result = Proxy.newProxyInstance(inter.getClassLoader(), new Class[] {inter},handler);
            proxies.put(new WeakReference(result), key);
        }
        System.out.println("--[java] ProxyFactory.getProxy we got proxy "+result);
        return result;
        } catch (Exception e) {
            System.out.println("--[java] ProxyFactory.getProxy we got exception "+e);
        }
        return null;
    }
    protected  static Hashtable proxies = null;
    private static Hashtable interfaces = null;
}




