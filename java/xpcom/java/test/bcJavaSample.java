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
        System.out.println("--[java]bcJavaSample::queryInterface result=null"+(result==null));
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
    /* void test5 (in nsIComponentManager cm); */
    public void test5(nsIComponentManager cm) {
        System.out.println("--[java]bcJavaSample.test5");
        try {
            nsIEnumerator [] retval = new nsIEnumerator[1];
            cm.enumerateContractIDs(retval);
            nsIEnumerator enumerator = retval[0];
            System.out.println("--[java] before calling enumerator.firts() "+
                               "enumerator==null "+(enumerator==null));

            enumerator.first();
            int counter = 0;
            nsISupports obj[] = new nsISupports[1];
            String str[] = new String[1];
            nsISupportsString strObj[] = new nsISupportsString[1];
            while (true) {
                enumerator.currentItem(obj);
                if (obj[0] == null ||
                    counter > 10) {
                    break;
                }
                obj[0].queryInterface(nsISupportsStringIID,strObj);
                strObj[0].toString(str);
                System.out.println("--[java] bcJavaSample.Test5 string "+str[0]);
                enumerator.next(); counter++;
            }
        } catch (Exception e) {
            System.out.println(e);
        }
    }

    static IID bcIJavaSampleIID = new IID(bcIJavaSample.IID);
    static IID nsISupportsIID = new IID(nsISupports.IID);
    static IID nsIComponenstManagerIID = new IID(nsIComponentManager.IID);
    static IID nsISupportsStringIID = new IID(nsISupportsString.IID);
    static {
      try {
          Class nsIComponentManagerClass = 
              Class.forName("org.mozilla.xpcom.nsIComponentManager");
          Class nsIEnumeratorClass = 
              Class.forName("org.mozilla.xpcom.nsIEnumerator");
          Class nsISupportsStringClass = 
              Class.forName("org.mozilla.xpcom.nsISupportsString");
          InterfaceRegistry.register(nsIComponentManagerClass);
          InterfaceRegistry.register(nsIEnumeratorClass);
          InterfaceRegistry.register(nsISupportsStringClass);
      } catch (Exception e) {
          System.out.println(e);
      }
    }

};






