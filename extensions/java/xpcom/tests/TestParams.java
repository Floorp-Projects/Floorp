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
 * Portions created by the Initial Developer are Copyright (C) 2005
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

import org.mozilla.xpcom.*;
import java.io.*;


public class TestParams
{
  public static final String TESTPARAMS_CID =
    "{3f64f1ad-bbbc-4c2f-88c8-e5b6b67bb0cc}";

  public static void main(String [] args)
  {
    System.loadLibrary("javaxpcom");

    String mozillaPath = System.getProperty("MOZILLA_FIVE_HOME");
    if (mozillaPath == null) {
      throw new RuntimeException("MOZILLA_FIVE_HOME system property not set.");
    }

    File localFile = new File(mozillaPath);
    XPCOM.initXPCOM(localFile, null);

    nsIComponentManager componentManager = XPCOM.getComponentManager();
    ITestParams p = (ITestParams)
      componentManager.createInstance(TESTPARAMS_CID, null,
                                      ITestParams.ITESTPARAMS_IID);
    if (p == null) {
      throw new RuntimeException("Failed to create ITestParams.");
    }

    testArrayParams(p);

    XPCOM.shutdownXPCOM(null);
  }

  static void testArrayParams(ITestParams p)
  {
    testArrayParamsIn(p);
    testArrayParamsOut(p);
    testArrayParamsInOut(p);
    testArrayParamsRetval(p);
  }

  static void testArrayParamsIn(ITestParams p)
  {
    int count = 4;
    byte[] byte_array = new byte[count];
    for (byte i = 0; i < count; i++) {
      byte_array[i] = i;
    }
    p.testSimpleTypeArrayIn(count, byte_array);

    count = 3;
    String[] str_array = new String[count];
    str_array[0] = "three";
    str_array[1] = "two";
    str_array[2] = "one";
    p.testCharStrTypeArrayIn(count, str_array);

    count = 4;
    str_array = new String[count];
    str_array[0] = "foúr";
    str_array[1] = "threë";
    str_array[2] = "twò";
    str_array[3] = "ône";
    p.testWCharStrTypeArrayIn(count, str_array);

    count = 2;
    String[] iid_array = new String[count];
    iid_array[0] = nsISupports.NS_ISUPPORTS_IID;
    iid_array[1] = nsISupportsWeakReference.NS_ISUPPORTSWEAKREFERENCE_IID;
    p.testIIDTypeArrayIn(count, iid_array);

    count = 3;
    nsILocalFile[] iface_array = new nsILocalFile[count];
    iface_array[0] = XPCOM.newLocalFile("/usr/bin", false);
    iface_array[1] = XPCOM.newLocalFile("/var/log", false);
    iface_array[2] = XPCOM.newLocalFile("/home", false);
    p.testIfaceTypeArrayIn(count, iface_array);
  }

  static void testArrayParamsOut(ITestParams p)
  {
    int[] count = new int[1];
    char[][] char_array = new char[1][];
    p.testSimpleTypeArrayOut(count, char_array);
    for (int i = 0; i < count[0]; i++) {
      System.out.println("[" + i + "]  " + char_array[0][i]);
    }

    String[][] str_array = new String[1][];
    p.testCharStrTypeArrayOut(count, str_array);
    for (int i = 0; i < count[0]; i++) {
      System.out.println("[" + i + "]  " + str_array[0][i]);
    }

    str_array = new String[1][];
    p.testWCharStrTypeArrayOut(count, str_array);
    for (int i = 0; i < count[0]; i++) {
      System.out.println("[" + i + "]  " + str_array[0][i]);
    }

    str_array = new String[1][];
    p.testIIDTypeArrayOut(count, str_array);
    for (int i = 0; i < count[0]; i++) {
      System.out.println("[" + i + "]  " + str_array[0][i]);
    }

    nsILocalFile[][] file_array = new nsILocalFile[1][];
    p.testIfaceTypeArrayOut(count, file_array);
    for (int i = 0; i < count[0]; i++) {
      System.out.println("[" + i + "]  " + file_array[0][i].getPath());
    }
  }

  static void testArrayParamsInOut(ITestParams p)
  {
    int count = 5;
    short[][] short_array = new short[1][count];
    for (short i = 0; i < count; i++) {
      short_array[0][i] = i;
    }
    p.testSimpleTypeArrayInOut(count, short_array);
    System.out.println("out:");
    for (int i = 0; i < count; i++) {
      System.out.println("[" + i + "]  " + short_array[0][i]);
    }

    count = 3;
    String[][] str_array = new String[1][count];
    str_array[0][0] = "three";
    str_array[0][1] = "two";
    str_array[0][2] = "one";
    p.testCharStrTypeArrayInOut(count, str_array);
    System.out.println("out:");
    for (int i = 0; i < count; i++) {
      System.out.println("[" + i + "]  " + str_array[0][i]);
    }

    count = 4;
    str_array = new String[1][count];
    str_array[0][0] = "foúr";
    str_array[0][1] = "threë";
    str_array[0][2] = "twò";
    str_array[0][3] = "ône";
    p.testWCharStrTypeArrayInOut(count, str_array);
    System.out.println("out:");
    for (int i = 0; i < count; i++) {
      System.out.println("[" + i + "]  " + str_array[0][i]);
    }

    count = 2;
    String[][] iid_array = new String[1][count];
    iid_array[0][0] = nsISupports.NS_ISUPPORTS_IID;
    iid_array[0][1] = nsISupportsWeakReference.NS_ISUPPORTSWEAKREFERENCE_IID;
    p.testIIDTypeArrayInOut(count, iid_array);
    System.out.println("out:");
    for (int i = 0; i < count; i++) {
      System.out.println("[" + i + "]  " + iid_array[0][i]);
    }

    count = 3;
    nsILocalFile[][] iface_array = new nsILocalFile[1][count];
    iface_array[0][0] = XPCOM.newLocalFile("/usr/bin", false);
    iface_array[0][1] = XPCOM.newLocalFile("/var/log", false);
    iface_array[0][2] = XPCOM.newLocalFile("/home", false);
    p.testIfaceTypeArrayInOut(count, iface_array);
    System.out.println("out:");
    for (int i = 0; i < count; i++) {
      System.out.println("[" + i + "]  " + iface_array[0][i].getPath());
    }
  }

  static void testArrayParamsRetval(ITestParams p)
  {
    int[] count = new int[1];
    int[] int_array = p.returnSimpleTypeArray(count);
    for (int i = 0; i < count[0]; i++) {
      System.out.println("[" + i + "]  " + int_array[i]);
    }

    String[] str_array = p.returnCharStrTypeArray(count);
    for (int i = 0; i < count[0]; i++) {
      System.out.println("[" + i + "]  " + str_array[i]);
    }

    str_array = p.returnWCharStrTypeArray(count);
    for (int i = 0; i < count[0]; i++) {
      System.out.println("[" + i + "]  " + str_array[i]);
    }

    str_array = p.returnIIDTypeArray(count);
    for (int i = 0; i < count[0]; i++) {
      System.out.println("[" + i + "]  " + str_array[i]);
    }

    nsILocalFile[] file_array = (nsILocalFile[]) p.returnIfaceTypeArray(count);
    for (int i = 0; i < count[0]; i++) {
      System.out.println("[" + i + "]  " + file_array[i].getPath());
    }
  }
}

