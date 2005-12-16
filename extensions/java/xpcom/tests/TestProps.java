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
import java.util.*;


/**
 * Simple testcase that uses the nsIProperties interface to test the JavaXPCOM
 * code.  In particular, it tests for 'out' and 'array' params (see method call
 * to getKeys).
 */

public class TestProps {
  public static final String NS_PROPERTIES_CONTRACTID = "@mozilla.org/properties;1";

  public static void main(String [] args) throws Exception {
    System.loadLibrary("javaxpcom");

    String mozillaPath = System.getProperty("MOZILLA_FIVE_HOME");
    if (mozillaPath == null) {
      throw new RuntimeException("MOZILLA_FIVE_HOME system property not set.");
    }

    File localFile = new File(mozillaPath);
    XPCOM.initXPCOM(localFile, null);
    // XPCOM.initXPCOM() only initializes XPCOM.  If you want to initialize
    // Gecko, you would do the following instead:
    //    GeckoEmbed.initEmbedding(localFile, null);

    nsIComponentManager componentManager = XPCOM.getComponentManager();
    nsIProperties props = (nsIProperties)
      componentManager.createInstanceByContractID(NS_PROPERTIES_CONTRACTID, null,
                                                  nsIProperties.NS_IPROPERTIES_IID);
    if (props == null) {
      throw new RuntimeException("Failed to create nsIProperties.");
    }

    // create the nsISupports objects we will use
    nsILocalFile localFile1 = XPCOM.newLocalFile("/user/local/share", false);
    nsILocalFile localFile2 = XPCOM.newLocalFile("/home/foo", false);
    nsILocalFile localFile3 = XPCOM.newLocalFile("/home/foo/bar", false);

    // set the properties and associate with the created objects
    props.set("File One", localFile1);
    props.set("File Two", localFile2);
    props.set("File One Repeated", localFile1);
    props.set("File Three", localFile3);

    // test the "has" method
    boolean hasProp = props.has("File One");
    if (hasProp == false)
      throw new NoSuchElementException("Could not find property 'File One'.");
    hasProp = props.has("File One Repeated");
    if (hasProp == false)
      throw new NoSuchElementException("Could not find property 'File One Repeated'.");
    hasProp = props.has("Nonexistant Property");
    if (hasProp == true)
      throw new Exception("Found property that doesn't exist.");

    // test the "get" method
    nsILocalFile tempLocalFile = (nsILocalFile) props.get("File One Repeated",
                                                          nsILocalFile.NS_ILOCALFILE_IID);
    if (tempLocalFile == null)
      throw new NoSuchElementException("Property 'File One Repeated' not found.");
    if (tempLocalFile != localFile1)
      throw new Exception("Object returned by 'get' not the same as object passed to 'set'.");

    // test the "undefine" method
    hasProp = props.has("File Two");
    if (hasProp == false)
      throw new NoSuchElementException();
    props.undefine("File Two");
    hasProp = props.has("File Two");
    if (hasProp == true)
      throw new NoSuchElementException();

    // test the "getKeys" method
    long[] count = new long[1];
    String[] keys = props.getKeys(count);
    if (keys == null || keys.length != 3) {
      System.out.println("getKeys returned incorrect array.");
    }
    for (int i = 0; i < keys.length; i++) {
      System.out.println("key " + i + ": " + keys[i]);
    }

    XPCOM.shutdownXPCOM(null);
    //    GeckoEmbed.termEmbedding();

    System.out.println("Test Passed.");
  }
}

