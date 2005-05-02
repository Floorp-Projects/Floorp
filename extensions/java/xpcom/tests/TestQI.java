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

import org.mozilla.xpcom.*;
import java.io.*;
import java.util.*;


/**
 * Simple test for the <code>XPCOM.queryInterface</code> method.  It should be
 * able to QI any interfaces implemented by the given object or its superclass
 * (and so on up the inheritance chain).
 *
 * @see XPCOM#queryInterface
 */

public class TestQI {

  public static void main(String [] args) throws Exception {
    System.loadLibrary("javaxpcom");

    String mozillaPath = System.getProperty("MOZILLA_FIVE_HOME");
    if (mozillaPath == null) {
      throw new RuntimeException("MOZILLA_FIVE_HOME system property not set.");
    }

    File localFile = new File(mozillaPath);
    XPCOM.initXPCOM(localFile, null);

    FooFile foo = new FooFile();

    nsIFileURL fileURL = (nsIFileURL) foo.queryInterface(nsIFileURL.NS_IFILEURL_IID);
    if (fileURL == null) {
      throw new RuntimeException("Failed to QI to nsIFileURL.");
    }

    nsIURL url = (nsIURL) foo.queryInterface(nsIURL.NS_IURL_IID);
    if (url == null) {
      throw new RuntimeException("Failed to QI to nsIURL.");
    }

    nsIURI uri = (nsIURI) foo.queryInterface(nsIURI.NS_IURI_IID);
    if (uri == null) {
      throw new RuntimeException("Failed to QI to nsIURI.");
    }

    nsISupports supp = (nsISupports) foo.queryInterface(nsISupports.NS_ISUPPORTS_IID);
    if (supp == null) {
      throw new RuntimeException("Failed to QI to nsISupports.");
    }

    XPCOM.shutdownXPCOM(null);

    System.out.println("Test Passed.");
  }
}


/**
 * Dummy class that implements nsIFileUrl.  The inheritance chain for
 * nsIFileURL is as follows:
 * <pre>
 *   nsIFileURL -> nsIURL -> nsIURI -> nsISupports
 * </pre>
 * <p>
 * The only method that is implemented is <code>queryInterface</code>, which
 * simply calls <code>XPCOM.queryInterface</code>.
 * </p>
 *
 * @see XPCOM#queryInterface
 */
class FooFile implements nsIFileURL {

  public nsISupports queryInterface(String aIID) {
      return XPCOM.queryInterface(this, aIID);
  }

	public nsIFile getFile() {
		return null;
	}

	public void setFile(nsIFile aFile) {
	}

	public String getFilePath() {
		return null;
	}

	public void setFilePath(String aFilePath) {
	}

	public String getParam() {
		return null;
	}

	public void setParam(String aParam) {
	}

	public String getQuery() {
		return null;
	}

	public void setQuery(String aQuery) {
	}

	public String getRef() {
		return null;
	}

	public void setRef(String aRef) {
	}

	public String getDirectory() {
		return null;
	}

	public void setDirectory(String aDirectory) {
	}

	public String getFileName() {
		return null;
	}

	public void setFileName(String aFileName) {
	}

	public String getFileBaseName() {
		return null;
	}

	public void setFileBaseName(String aFileBaseName) {
	}

	public String getFileExtension() {
		return null;
	}

	public void setFileExtension(String aFileExtension) {
	}

	public String getCommonBaseSpec(nsIURI aURIToCompare) {
		return null;
	}

	public String getRelativeSpec(nsIURI aURIToCompare) {
		return null;
	}

	public String getSpec() {
		return null;
	}

	public void setSpec(String aSpec) {
	}

	public String getPrePath() {
		return null;
	}

	public String getScheme() {
		return null;
	}

	public void setScheme(String aScheme) {
	}

	public String getUserPass() {
		return null;
	}

	public void setUserPass(String aUserPass) {
	}

	public String getUsername() {
		return null;
	}

	public void setUsername(String aUsername) {
	}

	public String getPassword() {
		return null;
	}

	public void setPassword(String aPassword) {
	}

	public String getHostPort() {
		return null;
	}

	public void setHostPort(String aHostPort) {
	}

	public String getHost() {
		return null;
	}

	public void setHost(String aHost) {
	}

	public int getPort() {
		return 0;
	}

	public void setPort(int aPort) {
	}

	public String getPath() {
		return null;
	}

	public void setPath(String aPath) {
	}

	public boolean equals(nsIURI other) {
		return false;
	}

	public boolean schemeIs(String scheme) {
		return false;
	}

	public nsIURI clone_() {
		return null;
	}

	public String resolve(String relativePath) {
		return null;
	}

	public String getAsciiSpec() {
		return null;
	}

	public String getAsciiHost() {
		return null;
	}

	public String getOriginCharset() {
		return null;
	}
}

