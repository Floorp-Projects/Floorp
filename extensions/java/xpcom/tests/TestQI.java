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
import java.io.FileFilter;
import java.io.IOException;

import org.mozilla.xpcom.Mozilla;
import org.mozilla.interfaces.nsIFile;
import org.mozilla.interfaces.nsIFileURL;
import org.mozilla.interfaces.nsIServiceManager;
import org.mozilla.interfaces.nsISupports;
import org.mozilla.interfaces.nsIURI;
import org.mozilla.interfaces.nsIURL;


/**
 * Simple test for the <code>XPCOM.queryInterface</code> method.  It should be
 * able to QI any interfaces implemented by the given object or its superclass
 * (and so on up the inheritance chain).
 *
 * @see XPCOM#queryInterface
 */

public class TestQI {

	private static File grePath;

	/**
	 * @param args	0 - full path to XULRunner binary directory
	 */
	public static void main(String[] args) {
		try {
			checkArgs(args);
		} catch (IllegalArgumentException e) {
			System.exit(-1);
		}

		Mozilla mozilla = Mozilla.getInstance();
		mozilla.initialize(grePath);

		File profile = null;
		nsIServiceManager servMgr = null;
		try {
			profile = createTempProfileDir();
			LocationProvider locProvider = new LocationProvider(grePath,
					profile);
			servMgr = mozilla.initXPCOM(grePath, locProvider);
		} catch (IOException e) {
			e.printStackTrace();
			System.exit(-1);
		}

		try {
			runTest();
		} catch (Exception e) {
			e.printStackTrace();
			System.exit(-1);
		}

		System.out.println("Test Passed.");

		// cleanup
		mozilla.shutdownXPCOM(servMgr);
		deleteDir(profile);
	}

	private static void runTest() {
		FooFile foo = new FooFile();

		nsIFileURL fileURL = (nsIFileURL) foo
				.queryInterface(nsIFileURL.NS_IFILEURL_IID);
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

		nsISupports supp = (nsISupports) foo
				.queryInterface(nsISupports.NS_ISUPPORTS_IID);
		if (supp == null) {
			throw new RuntimeException("Failed to QI to nsISupports.");
		}
	}

	private static void checkArgs(String[] args) {
		if (args.length != 1) {
			printUsage();
			throw new IllegalArgumentException();
		}

		grePath = new File(args[0]);
		if (!grePath.exists() || !grePath.isDirectory()) {
			System.err.println("ERROR: given path doesn't exist");
			printUsage();
			throw new IllegalArgumentException();
		}
	}

	private static void printUsage() {
		// TODO Auto-generated method stub
	}

	private static File createTempProfileDir() throws IOException {
		// Get name of temporary profile directory
		File profile = File.createTempFile("mozilla-test-", null);
		profile.delete();

		// On some operating systems (particularly Windows), the previous
		// temporary profile may not have been deleted. Delete them now.
		File[] files = profile.getParentFile()
				.listFiles(new FileFilter() {
					public boolean accept(File file) {
						if (file.getName().startsWith("mozilla-test-")) {
							return true;
						}
						return false;
					}
				});
		for (int i = 0; i < files.length; i++) {
			deleteDir(files[i]);
		}

		// Create temporary profile directory
		profile.mkdir();

		return profile;
	}

	private static void deleteDir(File dir) {
		File[] files = dir.listFiles();
		for (int i = 0; i < files.length; i++) {
			if (files[i].isDirectory()) {
				deleteDir(files[i]);
			}
			files[i].delete();
		}
		dir.delete();
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
		return Mozilla.queryInterface(this, aIID);
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

	public boolean _equals(nsIURI other) {
		return false;
	}

	public boolean schemeIs(String scheme) {
		return false;
	}

	public nsIURI _clone() {
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
