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
    public void queryInterface(IID iid, Object[] result) {
        System.out.println("--[java]bcJavaSample::queryInterface iid="+iid);
        if ( iid.equals(nsISupportsIID)
             || iid.equals(bcIJavaSampleIID)) {
            result[0] = this;
        } else {
            result = null;
        }
    }
    public void test0() {
        System.out.println("--[java]bcJavaSample.test0 ");
    }
    public void test1(int l) {
        System.out.println("--[java]bcJavaSample.test1 "+l+"\n");
    }
    public void test2(bcIJavaSample o) {
        System.out.println("--[java]bcJavaSample.test2");
        System.out.println("--[java]bcJavaSample.test2 :)))) Hi there");
        if (o != null) {
            System.out.println("--[java]bcJavaSample.test2 o!= null");
            o.test0();
            o.test1(1000);
            o.test2(this);
            int[] array={3,2,1};
            o.test3(3,array);
        } else {
            System.out.println("--[java]bcJavaSample.test2 o = null");
        }
    }
    public void test3(int count, int[] valueArray) {
        System.out.println("--[java]bcJavaSample.test3");
        System.out.println(valueArray.length);
        for (int i = 0; i < valueArray.length; i++) {
            System.out.println("--[java]callMethodByIndex args["+i+"] = "+valueArray[i]);
        }

    }
    public void test4(int count, String[][] valueArray) {
        System.out.println("--[java]bcJavaSample.test4");
        String[] array = valueArray[0];
        for (int i = 0; i < array.length; i++) {
            System.out.println("--[java]callMethodByIndex args["+i+"] = "+array[i]);
        }
        String[] returnArray = {"4","3","2","1"};
        valueArray[0] = returnArray;
    }
    static IID bcIJavaSampleIID;
    static IID nsISupportsIID;
    static { 
        try {
            Method[] methods = null;
            Class bcIJavaSampleClass = Class.forName("bcIJavaSample");
            Class IIDClass = Class.forName("org.mozilla.xpcom.IID");
            methods = new Method[100];
            Class ObjectArrayClass = (new Object[1]).getClass();
            Class IntArrayClass = (new int[1]).getClass();
            Class StringArrayArrayClass = (new String[1][1]).getClass();
            methods[0] = bcIJavaSampleClass.getDeclaredMethod("queryInterface",new Class[]{IIDClass,ObjectArrayClass});
            methods[3] = bcIJavaSampleClass.getDeclaredMethod("test0",new Class[]{});
            methods[4] = bcIJavaSampleClass.getDeclaredMethod("test1",new Class[]{Integer.TYPE});
            methods[5] = bcIJavaSampleClass.getDeclaredMethod("test2",new Class[]{bcIJavaSampleClass});
            methods[6] = bcIJavaSampleClass.getDeclaredMethod("test3",new Class[]{Integer.TYPE,IntArrayClass});
            methods[7] = bcIJavaSampleClass.getDeclaredMethod("test4",new Class[]{Integer.TYPE,StringArrayArrayClass});
            System.out.println(methods[0]+" "+methods[3]+" "+methods[4]+" "+methods[5]+" "+methods[6]+" "+methods[7]);
            bcIJavaSampleIID = new IID(bcIJavaSample.BC_IJAVASAMPLE_IID_STRING);
            nsISupportsIID = new IID("00000000-0000-0000-c000-000000000046");
            ProxyFactory.registerInterfaceForIID(bcIJavaSampleClass,bcIJavaSampleIID);
            new ProxyClass(bcIJavaSampleIID, methods); 
            //new ProxyClass(nsISupportsIID, methods); 
        } catch (Exception e) {
            System.out.println(e);
        }
    }
};

