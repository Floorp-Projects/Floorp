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
import org.mozilla.xpcom.*;
import java.lang.reflect.*;
public class bcJavaSample implements bcIJavaSample {
    public bcJavaSample() {
        System.out.println("--[java]bcJavaSample constructor");
    }
    public void test0() {
	System.out.println("--[java]bcJavaSample.test0 ");
    }
    public void test1(int l) {
	try {
	    System.out.println("--[java]bcJavaSample.test1 "+l);
	    System.out.println("--[java]bcJavaSample.test1\n :)))) Hey Hong"+l);
	    Object obj = ProxyFactory.getProxy(0,iid,0);
	    if (obj instanceof bcIJavaSample) {
		bcIJavaSample proxy = (bcIJavaSample)obj;
		proxy.test2(null);
	    } 
	} catch (Exception e) {
	    e.printStackTrace();
	}
    }
    public void test2(bcIJavaSample o) {
        System.out.println("--[java]bcJavaSample.test2");
        System.out.println("--[java]bcJavaSample.test2 :)))) Hi there");
        if (o != null) {
            System.out.println("--[java]bcJavaSample.test2 o!= null");
	    o.test0();
        } else {
            System.out.println("--[java]bcJavaSample.test2 o== null");
        }
    }
    static IID iid;
    static { 
        try {
            Method[] methods = null;
            Class bcIJavaSampleClass = Class.forName("bcIJavaSample");
            methods = new Method[3];
            methods[0] = bcIJavaSampleClass.getDeclaredMethod("test0",new Class[]{});
            methods[1] = bcIJavaSampleClass.getDeclaredMethod("test1",new Class[]{Integer.TYPE});
            methods[2] = bcIJavaSampleClass.getDeclaredMethod("test2",new Class[]{bcIJavaSampleClass});
            System.out.println(methods[0]+" "+methods[1]+" "+methods[2]);
            iid = new IID(bcIJavaSample.BC_IJAVASAMPLE_IID_STRING);
            ProxyFactory.registerInterfaceForIID(bcIJavaSampleClass,iid);
            new ProxyClass(iid, methods); 
        } catch (Exception e) {
            
        }
    }
};
