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
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
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

package org.mozilla.xpcom.internal;

import java.io.File;

import org.mozilla.xpcom.IJavaXPCOMUtils;


public class JavaXPCOMMethods implements IJavaXPCOMUtils {

  public static void registerJavaXPCOMMethods(File aLibXULDirectory) {
    // load JNI library
    String path = "";
    if (aLibXULDirectory != null) {
      path = aLibXULDirectory + File.separator;
    }

    String osName = System.getProperty("os.name").toLowerCase();
    if (osName.startsWith("os/2")) {
      System.load(path + System.mapLibraryName("jxpcmglu"));
    } else {
      System.load(path + System.mapLibraryName("javaxpcomglue"));
    }

    registerJavaXPCOMMethodsNative(aLibXULDirectory);
  }

  public static native void
      registerJavaXPCOMMethodsNative(File aLibXULDirectory);

  /**
   * Returns the Class object associated with the class or interface with the
   * given string name, using the class loader of the given object.
   *
   * @param aObject     the Java object whose class loader is used to load class
   * @param aClassName  the fully qualified name of desired class
   * @return  the Class object of requested Class; <code>null</code> if the
   *          class was not found
   *
   * @see http://java.sun.com/j2se/1.3/docs/guide/jni/jni-12.html#classops
   */
  public static Class findClassInLoader(Object aObject, String aClassName) {
    try {
      if (aObject == null) {
        return Class.forName(aClassName);
      } else {
        return Class.forName(aClassName, true,
            aObject.getClass().getClassLoader());
      }
    } catch (ClassNotFoundException e) {
      return null;
    }
  }

  public native long wrapJavaObject(Object aJavaObject, String aIID);

  public native Object wrapXPCOMObject(long aXPCOMObject, String aIID);

}

