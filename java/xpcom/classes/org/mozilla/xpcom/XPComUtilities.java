/*
 * The contents of this file are subject to the Mozilla Public License 
 * Version 1.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at 
 * http://www.mozilla.org/MPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis, 
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for 
 * the specific language governing rights and limitations under the License.
 *
 * Contributors:
 *    Frank Mitchell (frank.mitchell@sun.com)
 */
package org.mozilla.xpcom;

public class XPComUtilities {
    private static boolean didInit = false;

    static {
	initXPCOM();
    }

    static void initXPCOM() {
	if (didInit) return;

	didInit = true;
	System.loadLibrary("xpjava");
	if (!initialize()) {
	    System.out.println("Initialization failed!!");
	    System.exit(-1);
	}
    }

    private static native boolean initialize();

    // public static native nsIComponentManager GetGlobalComponentManager();

    public static native nsISupports CreateInstance(nsID classID, 
						    nsISupports aDelegate, 
						    nsID interfaceID);

    /*
    public static native Object CreateInstance(String progID, 
					       nsISupports aDelegate, 
					       nsID interfaceID);

    public static native void RegisterFactory(nsID classID,
					      String className,
					      String progID,
					      nsIFactory aFactory, 
					      boolean replace);
    */

    public static native void RegisterComponent(nsID classID,
						String className,
						String progID,
						String libraryName,
						boolean replace, 
						boolean persist);

    //public static native nsIInterfaceInfoManager GetInterfaceInfoManager();

    static native void AddRef(long ref);
    static native void Release(long ref);

    static native void InvokeMethodByIndex(nsID iid,
					   long ref, 
					   int index, 
					   Object[] args);

    // TEMPORARY: remove ASAP
    public static void InvokeMethodByIndex(nsID iid,
					   ComObject object,
					   int index, 
					   Object[] args) {
	object.__invokeByIndex(iid, index, args);
    }
}
