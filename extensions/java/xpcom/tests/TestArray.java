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
import org.mozilla.xpcom.XPCOMException;
import org.mozilla.interfaces.nsIComponentManager;
import org.mozilla.interfaces.nsIMutableArray;
import org.mozilla.interfaces.nsIServiceManager;
import org.mozilla.interfaces.nsISupports;

/**
 * Adapted from xpcom/tests/TestArray.cpp
 *
 * This tests the following parts of the Java<->XPCOM code:
 *    - Creating an XPCOM object in Java, and calling its methods.
 *    - Calling several of the Gecko embedding funtions.
 *    - Passing Java class objects to XPCOM.
 *    - Have XPCOM call Java class' queryInterface() method.
 *    - Catching GeckoException.
 *    - Lifetime of JNI structures and Java objects (everything should be
 *      garbage collected or destroyed at the appropriate time).
 *
 * NOTE: The System.gc() calls throughout the testcase are used to tell the
 *    JVM that now would be a good time to garbage collect.  This helps us see
 *    if everything is getting freed properly.  However, since the System.gc()
 *    call is only a suggestion to the JVM (you can't force garbage collection),
 *    this testcase may not work the same on every system.  On my system, it
 *    usually works as expected (that is, Java objects that are no longer in
 *    use are destroyed whenever System.gc() is called).  But I have seen some
 *    instances where the garbage collector ran later in the testcase.
 */

public class TestArray {

	public static final String NS_ARRAY_CONTRACTID = "@mozilla.org/array;1";

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

		System.out.println("\n--> initialized\n");

		try {
			runTest();
		} catch (Exception e) {
			e.printStackTrace();
			System.exit(-1);
		}
		
		System.gc();

		// cleanup
		mozilla.shutdownXPCOM(servMgr);
		deleteDir(profile);

		System.out.println("Test Passed.");
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

	private static void runTest() {
		Mozilla mozilla = Mozilla.getInstance();
		nsIComponentManager componentManager = mozilla.getComponentManager();
		nsIMutableArray array = (nsIMutableArray) componentManager
				.createInstanceByContractID(NS_ARRAY_CONTRACTID, null,
						nsIMutableArray.NS_IMUTABLEARRAY_IID);
		if (array == null) {
			throw new RuntimeException("Failed to create nsIMutableArray.");
		}

		fillArray(array, 10);
		System.out.println("Array created:");
		int fillResult[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
		dumpArray(array, 10, fillResult, 10);

		// test insert
		Foo foo = (Foo) array.queryElementAt(3, nsISupports.NS_ISUPPORTS_IID);
		array.insertElementAt(foo, 5, false);
		System.out.println("insert 3 at 5:");
		int insertResult[] = {0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9};
		dumpArray(array, 11, insertResult, 10);
		System.out.println("insert 3 at 0:");
		array.insertElementAt(foo, 0, false);
		int insertResult2[] = {3, 0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9};
		dumpArray(array, 12, insertResult2, 10);
		System.out.println("append 3:");
		array.appendElement(foo, false);
		int appendResult[] = {3, 0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9, 3};
		dumpArray(array, 13, appendResult, 10);

		// test IndexOf && LastIndexOf
		int expectedIndex[] = {0, 4, 6, 12, -1};
		int count = 0;
		long index = array.indexOf(0, foo);
		System.out.println("IndexOf(foo): " + index + "=" + expectedIndex[count] +
				" " + assertEqual(index, expectedIndex[count]));
		try {
			do {
				count++;
				index = array.indexOf(index + 1, foo);
				System.out.println("IndexOf(foo): " + index + "=" +
						expectedIndex[count] + " " +
						assertEqual(index, expectedIndex[count]));
			} while (true);
		} catch (XPCOMException e) {
			// If array.indexOf() did not find the element, it returns
			// NS_ERROR_FAILURE.
			if (e.errorcode != Mozilla.NS_ERROR_FAILURE) {
				throw e;
			}
		}

		index = lastIndexOf(array, foo);
		count--;
		System.out.println("LastIndexOf(foo): " + index + "=" +
				expectedIndex[count] + " " + 
				assertEqual(index, expectedIndex[count]));

		System.out.println("ReplaceElementAt(8):");
		replaceElementAt(array, foo, 8);
		System.gc();
		int replaceResult[] = {3, 0, 1, 2, 3, 4, 3, 5, 3, 7, 8, 9, 3};
		dumpArray(array, 13, replaceResult, 9);

		System.out.println("RemoveElementAt(0):");
		array.removeElementAt(0);
		System.gc();
		int removeResult[] = {0, 1, 2, 3, 4, 3, 5, 3, 7, 8, 9, 3};
		dumpArray(array, 12, removeResult, 9);
		System.out.println("RemoveElementAt(7):");
		array.removeElementAt(7);
		System.gc();
		int removeResult2[] = {0, 1, 2, 3, 4, 3, 5, 7, 8, 9, 3};
		dumpArray(array, 11, removeResult2, 9);
		System.out.println("RemoveElement(foo):");
		removeElement(array, foo);
		System.gc();
		int removeResult3[] = {0, 1, 2, 4, 3, 5, 7, 8, 9, 3};
		dumpArray(array, 10, removeResult3, 9);
		System.out.println("RemoveLastElement(foo):");
		removeLastElement(array, foo);
		System.gc();
		int removeResult4[] = {0, 1, 2, 4, 3, 5, 7, 8, 9};
		dumpArray(array, 9, removeResult4, 9);

		// test clear
		foo = null;  // remove ref now, so everything is cleared
		System.out.println("clear array:");
		array.clear();
		System.gc();
		dumpArray(array, 0, null, 0);
		System.out.println("add 4 new:");
		fillArray(array, 4);
		dumpArray(array, 4, fillResult, 4);

		// test deleting of array
		System.out.println("release array:");
		array = null;
		System.gc();
		dumpArray(array, 0, null, 0);

		componentManager = null;
	}

	static void fillArray(nsIMutableArray aArray, int aCount) {
		for (int index = 0; index < aCount; index++) {
			nsISupports foo = new Foo(index);
			aArray.appendElement(foo, false);
		}
	}

	static String assertEqual(long aValue1, long aValue2) {
		if (aValue1 == aValue2)
			return "OK";
		return "ERROR";
	}

	static void dumpArray(nsIMutableArray aArray, int aExpectedCount,
			int[] aElementIDs, int aExpectedTotal) {
		long count = 0;
		if (aArray != null)
			count = aArray.getLength();

		System.out.println("object count " + Foo.gCount + " = " + aExpectedTotal +
				" " + assertEqual(Foo.gCount, aExpectedTotal));
		System.out.println("array count " + count + " = " + aExpectedCount + " " +
				assertEqual(count, aExpectedCount));

		for (int index = 0; (index < count) && (index < aExpectedCount); index++) {
			Foo foo = (Foo) aArray.queryElementAt(index, nsISupports.NS_ISUPPORTS_IID);
			System.out.println(index + ": " + aElementIDs[index] + "=" +
					foo.getId() + " (" +
					Integer.toHexString(foo.hashCode()) + ") " + 
					assertEqual(foo.getId(), aElementIDs[index]));
			foo = null;
		}
	}

	static long lastIndexOf(nsIMutableArray aArray, nsISupports aElement) {
		for (long i = aArray.getLength() - 1; i >= 0; i--) {
			Foo foo = (Foo) aArray.queryElementAt(i, nsISupports.NS_ISUPPORTS_IID);
			if (foo == aElement)
				return i;
		}
		return -1;
	}

	static void replaceElementAt(nsIMutableArray aArray, nsISupports aElement,
			int aIndex) {
		aArray.removeElementAt(aIndex);
		aArray.insertElementAt(aElement, aIndex, false);
	}

	/* Removes first instance of given element */
	static void removeElement(nsIMutableArray aArray, nsISupports aElement) {
		long index = aArray.indexOf(0, aElement);
		aArray.removeElementAt(index);
	}

	static void removeLastElement(nsIMutableArray aArray, nsISupports aElement) {
		long index = lastIndexOf(aArray, aElement);
		aArray.removeElementAt(index);
	}
}

class Foo implements nsISupports {

	static int gCount;
	int mID;

	public Foo(int aID)
	{
		mID = aID;
		++gCount;
		System.out.println("init: " + mID + " (" +
				Integer.toHexString(this.hashCode()) + "), " +
				gCount +" total");
	}

	// nsISupports implementation
	public nsISupports queryInterface(String aIID)
	{
		return Mozilla.queryInterface(this, aIID);
	}

	public int getId()
	{
		return mID;
	}

	protected void finalize() throws Throwable
	{
		--gCount;
		System.out.println("destruct: " + mID + " (" +
				Integer.toHexString(this.hashCode()) + "), " +
				gCount +" remain");
	}
}
