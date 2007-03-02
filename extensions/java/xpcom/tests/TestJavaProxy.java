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
 * Portions created by the Initial Developer are Copyright (C) 2007
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

import java.io.File;
import java.io.FileNotFoundException;
import java.util.Properties;

import org.mozilla.xpcom.GREVersionRange;
import org.mozilla.xpcom.Mozilla;
import org.mozilla.interfaces.nsIFile;
import org.mozilla.interfaces.nsILocalFile;
import org.mozilla.interfaces.nsISimpleEnumerator;
import org.mozilla.interfaces.nsISupports;


/**
 * Tests that if calls to XPCOM functions return the same object, then
 * the JavaXPCOM interface creates the proper Java proxies.
 * <p>
 * The XPCOM call to <code>nsISupports supp = entries.getNext()</code> returns
 * an object, for which we create an <code>nsISupports</code> Java proxy.  Then,
 * the XPCOM call to <code>supp.queryInterface(nsIFile.NS_IFILE_IID)</code>
 * will return the same object (same address).  JavaXPCOM needs to be smart
 * enough to create a new <code>nsIFile</code> proxy, rather than reusing the
 * <code>nsISupports</code> one that was previously created.
 * </p>
 */
public class TestJavaProxy {
	public static void main(String [] args) throws Exception {
    GREVersionRange[] range = new GREVersionRange[1];
    range[0] = new GREVersionRange("1.8", true, "1.9+", true);
    Properties props = null;
      
    File grePath = null;
    try {
      grePath = Mozilla.getGREPathWithProperties(range, props);
    } catch (FileNotFoundException e) { }
      
    if (grePath == null) {
      System.out.println("found no GRE PATH");
      return;
    }
    System.out.println("GRE PATH = " + grePath.getPath());
    
    Mozilla Moz = Mozilla.getInstance();
    try {
      Moz.initXPCOM(grePath, null);
    } catch (IllegalArgumentException e) {
      System.out.println("no javaxpcom.jar found in given path");
      return;
    } catch (Throwable t) {
      System.out.println("initXPCOM failed");
      t.printStackTrace();
      return;
    }
    System.out.println("\n--> initialized\n");

		nsILocalFile directory = Moz.newLocalFile("/usr", false);
		nsISimpleEnumerator entries = (nsISimpleEnumerator)
        directory.getDirectoryEntries();
		while (entries.hasMoreElements()) {
			nsISupports supp = entries.getNext();
			nsIFile file = (nsIFile) supp.queryInterface(nsIFile.NS_IFILE_IID);
			System.out.println(file.getPath());
		}

    // cleanup
    directory = null;
    entries = null;

		Moz.shutdownXPCOM(null);
	}
}
