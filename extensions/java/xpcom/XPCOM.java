/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Java XPCOM Bindings.
 *
 * The Initial Developer of the Original Code is
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Pedemonte (jhpedemonte@gmail.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.xpcom;

import java.lang.reflect.*;
import java.io.*;


public final class XPCOM {

  public static native nsIServiceManager initXPCOM(File aMozBinDirectory, AppFileLocProvider aAppFileLocProvider);
  public static native void shutdownXPCOM(nsIServiceManager aServMgr);
  public static native nsIServiceManager getServiceManager();
  public static native nsIComponentManager getComponentManager();
  public static native nsIComponentRegistrar getComponentRegistrar();
  public static native nsILocalFile newLocalFile(String aPath, boolean followLinks);


  /*  Utility functions */

  // Generic QueryInterface implementation
  public static nsISupports queryInterface(nsISupports aObject, String aIID)
  {
    Class[] interfaces = aObject.getClass().getInterfaces();
    for (int i = 0; i < interfaces.length; i++ ) {
      if (aIID.equals(XPCOM.getInterfaceIID(interfaces[i])))
        return aObject;
    }
    return null;
  }

  // Given an interface, this will construct the name of the IID field (such as
  // NS_ISUPPORTS_IID) and return its value.
  public static String getInterfaceIID(Class aInterface)
  {
    // Get short class name (i.e. "bar", not "org.blah.foo.bar")
    StringBuffer iidName = new StringBuffer();
    String fullClassName = aInterface.getName();
    int index = fullClassName.lastIndexOf(".");
    String className = index > 0 ? fullClassName.substring(index + 1) :
                                    fullClassName;

    // Create iid field name
    if (className.startsWith("ns")) {
      iidName.append("NS_");
      iidName.append(className.substring(2).toUpperCase());
    } else {
      iidName.append(className.toUpperCase());
    }
    iidName.append("_IID");

    String iid;
    try {
      Field iidField = aInterface.getDeclaredField(iidName.toString());
      iid = (String) iidField.get(null);
    } catch (NoSuchFieldException e) {
      // Class may implement non-Mozilla interfaces, which would not have an
      // IID method.  In that case, just return an emptry string.
      iid = "";
    } catch (IllegalAccessException e) {
      // XXX Should be using a logging service, such as java.util.logging
      System.err.println("ERROR: Could not get field " + iidName.toString());
      iid = "";
    }

    return iid;
  }

}

