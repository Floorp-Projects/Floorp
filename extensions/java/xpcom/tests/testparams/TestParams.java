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
import java.lang.reflect.Method;

import org.mozilla.xpcom.Mozilla;
import org.mozilla.xpcom.XPCOMException;
import org.mozilla.interfaces.nsIEcho;
import org.mozilla.interfaces.nsIJXTestParams;
import org.mozilla.interfaces.nsIStackFrame;
import org.mozilla.interfaces.nsISupports;
import org.mozilla.interfaces.nsITestXPCFunctionCallback;
import org.mozilla.interfaces.nsITestXPCSomeUselessThing;
import org.mozilla.interfaces.nsIXPCTestIn;


/**
 * Tests the marshalling and unmarshalling of method parameters using the
 * XPConnect testcases from mozilla/js/src/xpconnect/tests.
 */
public class TestParams {

	private static File grePath;
	
	private static String testString = "this is a test string";
	private static String emptyString = "";
	private static String utf8String =
	   "Non-Ascii 1 byte chars: éâäàåç, 2 byte chars: \u1234 \u1235 \u1236";
	//private static String sharedString = "a shared string";


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
		try {
			profile = createTempProfileDir();
			LocationProvider locProvider = new LocationProvider(grePath,
					profile);
			mozilla.initEmbedding(grePath, grePath, locProvider);
		} catch (IOException e) {
			e.printStackTrace();
			System.exit(-1);
		}

		try {
			runEchoTests();

			runInTests();
		} catch (RunTestsException e) {
			System.exit(-1);
		}
		
		nsIJXTestParams jstest = (nsIJXTestParams) Mozilla.getInstance()
			.getComponentManager()
				.createInstanceByContractID("@mozilla.org/javaxpcom/tests/params;1",
						null, nsIJXTestParams.NS_IJXTESTPARAMS_IID);
		try {
			jstest.runTests(new JavaTests());
		} catch (XPCOMException e) {
			System.exit(-1);
		}


		// cleanup
		mozilla.termEmbedding();
		deleteDir(profile);
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

	/**
	 * @param aCallee
	 * @param aTests  An array of tests to run.  The format looks like this:
	 *                  index 0:  method name of test to run
	 *                        1:  Object array of method arguments
	 *                        2:  expected result
	 *                        3:  comment to use when logging test result; if
	 *                            <code>null</code>, uses method name
	 * @throws RunTestsException if any of the tests fails
	 */
	private static void runTestsArray(Object aCallee, Object[][] aTests)
	throws RunTestsException {
		boolean succeeded = true;
		for (int i = 0; i < aTests.length; i++) {
			Method method = getMethod(aCallee.getClass(), (String) aTests[i][0]);

			String comment = (aTests[i][3] != null) ? (String) aTests[i][3] :
				(String) aTests[i][0];

			Object result = null;
			boolean passed = false;
			try {
				result = method.invoke(aCallee, (Object[]) aTests[i][1]);
				if (result != null) {
					passed = (result.equals(aTests[i][2]));
				} else {
					passed = (result == aTests[i][2]);
				}
			} catch (Exception e) {
				succeeded = false;
				System.err.println("*** TEST " + comment + " threw exception:");
				e.printStackTrace();
				logResult(comment, false);
				continue;
			}

			logResult(comment, passed);

			if (!passed) {
				succeeded = false;
				System.err.println("*** TEST " + comment + " FAILED: expected "
						+ aTests[i][2] + ", returned " + result);
			}
		}
		
		if (!succeeded) {
			throw new RunTestsException();
		}
	}

	private static Method getMethod(Class aClass, String aMethodName) {
		Method[] methods = aClass.getMethods();
		for (int i = 0; i < methods.length; i++) {
			if (methods[i].getName().equals(aMethodName)) {
				return methods[i];
			}
		}
		return null;
	}

	private static void logResult(String comment, boolean passed) {
		System.out.println(comment + "\t\t" + (passed ? "passed" : "FAILED"));
	}

	private static void runEchoTests() throws RunTestsException {
		nsIEcho nativeEcho = (nsIEcho) Mozilla.getInstance()
			.getComponentManager()
				.createInstanceByContractID("@mozilla.org/javaxpcom/tests/xpc;1",
						null, nsIEcho.NS_IECHO_IID);

		Object[][] tests = new Object[][] {
			/* test DOMString */
			{ "in2OutOneDOMString", new Object[] { testString }, testString,
				"in2OutOneDOMString w/ '" + testString + "'" },
			{ "in2OutOneDOMString", new Object[] { emptyString }, emptyString,
				"in2OutOneDomString w/ empty string" },
			{ "in2OutOneDOMString", new Object[] { null }, null,
				"in2OutOneDOMString w/ null value" },
			/* test AString */
			{ "in2OutOneAString", new Object[] { testString }, testString,
				"in2OutOneAString w/ '" + testString + "'" },
			{ "in2OutOneAString", new Object[] { emptyString }, emptyString,
				"in2OutOneAString w/ empty string" },
			{ "in2OutOneAString", new Object[] { utf8String }, utf8String,
				"in2OutOneAString w/ '" + utf8String + "'" },
			{ "in2OutOneAString", new Object[] { null }, null,
				"in2OutOneAString w/ null value" },
			/* test AUTF8String */
			{ "in2OutOneUTF8String", new Object[] { testString }, testString,
				"in2OutOneUTF8String w/ '" + testString + "'" },
			{ "in2OutOneUTF8String", new Object[] { emptyString }, emptyString,
				"in2OutOneUTF8String w/ empty string" },
			{ "in2OutOneUTF8String", new Object[] { utf8String }, utf8String,
				"in2OutOneUTF8String w/ '" + utf8String + "'" },
			{ "in2OutOneUTF8String", new Object[] { null }, null,
				"in2OutOneUTF8String w/ null value" },
			/* test ACString */
			{ "in2OutOneCString", new Object[] { testString }, testString,
				"in2OutOneCString w/ '" + testString + "'" },
			{ "in2OutOneCString", new Object[] { emptyString }, emptyString,
				"in2OutOneCString w/ empty string" },
			{ "in2OutOneCString", new Object[] { null }, null,
				"in2OutOneCString w/ null value" },
			/* test normal strings */
			{ "in2OutOneString", new Object[] { testString }, testString,
				"in2OutOneString w/ '" + testString + "'" },
			{ "in2OutOneString", new Object[] { emptyString }, emptyString,
				"in2OutOneString w/ empty string" }
		};

		runTestsArray(nativeEcho, tests);

		do {
			Object result = null;
			boolean passed = false;
			String comment = "in2OutOneString w/ null value";
			try {
				result = nativeEcho.in2OutOneString(null);
				passed = (result == null);
			} catch (Exception e) {
				// Calling nativeEcho.in2OutOneString(null) should throw an
				// exception, with errorcode == NS_ERROR_FAILURE.
				if (e instanceof XPCOMException &&
						((XPCOMException) e).errorcode == Mozilla.NS_ERROR_FAILURE) {
					passed = true;
				}

				if (!passed) {
					System.err.println("*** TEST " + comment +
							" threw exception:");
					e.printStackTrace();
					logResult(comment, false);
					continue;
				}
			}

			logResult(comment, passed);

			if (!passed) {
				System.err.println("*** TEST " + comment +
						" FAILED: expected null, returned " + result);
			}
		} while (false);

		tests = new Object[][] {
			{ "setAString", new Object[] { testString }, null,
				"setAString w/ '" + testString + "'" },
			{ "getAString", null, testString,
				"getAString w/ '" + testString + "'" },
			{ "setAString", new Object[] { emptyString }, null,
				"setAString w/ empty string" },
			{ "getAString", null, emptyString,
				"getAString w/ empty string" },
			{ "setAString", new Object[] { null }, null,
				"setAString w/ null value" },
			{ "getAString", null, null,
				"getAString w/ null value" },
			/* Is the 'shared' attribute even used in scriptable functions?
        	{ "sharedString", null, sharedString, null } */
		};

		runTestsArray(nativeEcho, tests);
	}

	private static void runInTests() throws RunTestsException {
		nsIXPCTestIn nativeInTest = (nsIXPCTestIn) Mozilla.getInstance()
			.getComponentManager()
				.createInstanceByContractID("@mozilla.org/javaxpcom/tests/xpc;1",
						null, nsIXPCTestIn.NS_IXPCTESTIN_IID);

		/* XPConnect converts 64-bit values into doubles when calling into
		 * Javascript.  If we use Long.MIN_VALUE or Long.MAX_VALUE, the values
		 * are rounded 'up' when converted to double.  Then, when converted back
		 * to a 64-bit integer, the conversion overflows, always resulting in
		 * Long.MIN_VALUE.  To workaround that, we just call with very large
		 * values, but ones that won't overflow in conversion.
		 */
		//Object minLong = new Long(Long.MIN_VALUE);
		//Object maxLong = new Long(Long.MAX_VALUE);
		Object minLong = new Long(-9223372036854774784L);
		Object maxLong = new Long(9223372036854774784L);
		Object zeroLong = new Long(0);
		Object negLong = new Long(-1);
		Object minInt = new Integer(Integer.MIN_VALUE);
		Object maxInt = new Integer(Integer.MAX_VALUE);
		Object zeroInt = new Integer(0);
		Object negInt = new Integer(-1);
		Object maxUInt = new Long(4294967295L);
		Object minShort = new Short(Short.MIN_VALUE);
		Object maxShort = new Short(Short.MAX_VALUE);
		Object zeroShort = new Short((short) 0);
		Object negShort = new Short((short) -1);
		Object maxUShort = new Integer(65535);
		Object charA = new Character('a');
		Object charZ = new Character('Z');
		Object charWide = new Character('\u1234');
		Object maxUByte = new Short((short) 255);
		Object minFloat = new Float(Float.MIN_VALUE);
		Object maxFloat = new Float(Float.MAX_VALUE);
		Object minDouble = new Double(Double.MIN_VALUE);
		Object maxDouble = new Double(Double.MAX_VALUE);

		Object[][] tests = new Object[][] {
			{ "echoLong", new Object[] { minInt }, minInt,
				"echoLong w/ " + minInt },
			{ "echoLong", new Object[] { maxInt }, maxInt,
				"echoLong w/ " + maxInt },
			{ "echoShort", new Object[] { minShort }, minShort,
				"echoShort w/ " + minShort },
			{ "echoShort", new Object[] { maxShort }, maxShort,
				"echoShort w/ " + maxShort },
			{ "echoChar", new Object[] { charA }, charA,
				"echoChar w/ " + charA },
			{ "echoChar", new Object[] { charZ }, charZ,
				"echoChar w/ " + charZ },
			{ "echoBoolean", new Object[] { Boolean.FALSE }, Boolean.FALSE,
				"echoBoolean w/ " + Boolean.FALSE},
			{ "echoBoolean", new Object[] { Boolean.TRUE }, Boolean.TRUE,
				"echoBoolean w/ " + Boolean.TRUE },
		/* XXX bug 367793
			{ "echoOctet", new Object[] { zeroShort }, zeroShort,
				"echoOctet w/ zero"},
			{ "echoOctet", new Object[] { maxUByte }, maxUByte,
				"echoOctet w/ " + maxUByte },
		*/
			{ "echoLongLong", new Object[] { minLong }, minLong,
				"echoLongLong w/ " + minLong },
			{ "echoLongLong", new Object[] { maxLong }, maxLong,
				"echoLongLong w/ " + maxLong },
			{ "echoUnsignedShort", new Object[] { maxUShort }, maxUShort,
				"echoUnsignedShort w/ " + maxUShort },
			{ "echoUnsignedShort", new Object[] { zeroInt }, zeroInt,
				"echoUnsignedShort w/ zero" },
			// is this test case correct/valid?
			{ "echoUnsignedShort", new Object[] { negInt }, maxUShort,
				"echoUnsignedShort w/ -1" },
			{ "echoUnsignedLong", new Object[] { maxUInt }, maxUInt,
				"echoUnsignedLong w/ " + maxUInt },
			{ "echoUnsignedLong", new Object[] { zeroLong }, zeroLong,
				"echoUnsignedLong w/ zero" },
			{ "echoUnsignedLong", new Object[] { negLong }, maxUInt,
				"echoUnsignedLong w/ -1" },
			{ "echoFloat", new Object[] { minFloat }, minFloat,
				"echoFloat w/ " + minFloat },
			{ "echoFloat", new Object[] { maxFloat }, maxFloat,
				"echoFloat w/ " + maxFloat },
			{ "echoDouble", new Object[] { minDouble }, minDouble,
				"echoDouble w/ " + minDouble },
			{ "echoDouble", new Object[] { maxDouble }, maxDouble,
				"echoDouble w/ " + maxDouble },
			{ "echoWchar", new Object[] { charA }, charA,
				"echoWchar w/ " + charA },
			{ "echoWchar", new Object[] { charZ }, charZ,
				"echoWchar w/ " + charZ },
			{ "echoWchar", new Object[] { charWide }, charWide,
				"echoWchar w/ " + charWide },
			{ "echoString", new Object[] { testString }, testString,
				"echoString w/ '" + testString + "'" },
			{ "echoString", new Object[] { emptyString }, emptyString,
				"echoString w/ empty string" },
			{ "echoString", new Object[] { utf8String }, utf8String,
				"echoString w/ '" + utf8String + "'" },
			{ "echoString", new Object[] { null }, null,
				"echoString w/ null value" },
		};

		runTestsArray(nativeInTest, tests);

		// Max unsigned long long = 18446744073709551615UL
		Object maxULong = new Double(1.8446744073709550E19);
		Object zeroDouble = new Double(0.0);

		tests = new Object[][] {
			{ "echoPRBool", new Object[] { Boolean.FALSE }, Boolean.FALSE,
				"echoPRBool w/ false" },
			{ "echoPRBool", new Object[] { Boolean.TRUE }, Boolean.TRUE,
				"echoPRBool w/ true" },
			{ "echoPRInt32", new Object[] { minInt }, minInt,
				"echoPRInt32 w/ " + minInt },
			{ "echoPRInt32", new Object[] { maxInt }, maxInt,
				"echoPRInt32 w/ " + maxInt },
			{ "echoPRInt16", new Object[] { minShort }, minShort,
				"echoPRInt16 w/ " + minShort },
			{ "echoPRInt16", new Object[] { maxShort }, maxShort,
				"echoPRInt16 w/ " + maxShort },
			{ "echoPRInt64", new Object[] { minLong }, minLong,
				"echoPRInt64 w/ " + minLong },
			{ "echoPRInt64", new Object[] { maxLong }, maxLong,
				"echoPRInt64 w/ " + maxLong },
			{ "echoPRUint8", new Object[] { maxUByte }, maxUByte,
				"echoPRUint8 w/ " + maxUByte },
			{ "echoPRUint8", new Object[] { zeroShort }, zeroShort,
				"echoPRUint8 w/ zero" },
			{ "echoPRUint8", new Object[] { negShort }, maxUByte,
				"echoPRUint8 w/ -1" },
			{ "echoPRUint16", new Object[] { maxUShort }, maxUShort,
				"echoPRUint16 w/ " + maxUShort },
			{ "echoPRUint16", new Object[] { zeroInt }, zeroInt,
				"echoPRUint16 w/ zero" },
			{ "echoPRUint16", new Object[] { negInt }, maxUShort,
				"echoPRUint16 w/ -1" },
			{ "echoPRUint32", new Object[] { maxUInt }, maxUInt,
				"echoPRUint32 w/ " + maxUInt },
			{ "echoPRUint32", new Object[] { zeroLong }, zeroLong,
				"echoPRUint32 w/ zero" },
			{ "echoPRUint32", new Object[] { negLong }, maxUInt,
				"echoPRUint32 w/ -1" },
			{ "echoPRUint64", new Object[] { maxULong }, maxULong,
				"echoPRUint64 w/ " + maxULong },
			{ "echoPRUint64", new Object[] { zeroDouble }, zeroDouble,
				"echoPRUint64 w/ zero" },
			/* XXX
	        { "echoPRUint64", new Object[] { negInt }, maxULong,
	            "echoPRUint64 w/ -1" }, */
		};

		runTestsArray(nativeInTest, tests);
	}
}

class RunTestsException extends Exception {
	private static final long serialVersionUID = -2865653674109284035L;
}

class JavaTests implements nsIEcho, nsIXPCTestIn {

	private nsIEcho receiver;
	private String savedString;
	private Object[] results = new Object[8];

	/* nsISupports */

	public nsISupports queryInterface(String aIID) {
		return Mozilla.queryInterface(this, aIID);
	}

	/* nsIEcho */

	public void callFunction(nsITestXPCFunctionCallback callback, String s) {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void callFunctionWithThis(nsITestXPCFunctionCallback callback,
			nsISupports self, String s) {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void callReceiverSometimeLater() {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void debugDumpJSStack() {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public String echoIn2OutOneAString(String input) {
		if (receiver != null) {
			return receiver.echoIn2OutOneAString(input);
		}
		return in2OutOneAString(input);
	}

	public String echoIn2OutOneCString(String input) {
		if (receiver != null) {
			return receiver.echoIn2OutOneCString(input);
		}
		return in2OutOneCString(input);
	}

	public String echoIn2OutOneDOMString(String input) {
		if (receiver != null) {
			return receiver.echoIn2OutOneDOMString(input);
		}
		return in2OutOneDOMString(input);
	}

	public String echoIn2OutOneUTF8String(String input) {
		if (receiver != null) {
			return receiver.echoIn2OutOneUTF8String(input);
		}
		return in2OutOneUTF8String(input);
	}

	public void failInJSTest(int fail) {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public String getAString() {
		return savedString;
	}

	public int getSomeValue() {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public nsIStackFrame getStack() {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public short getThrowInGetter() {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public int in2OutAddTwoInts(int input1, int input2, int[] output1,
			int[] output2) {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public String in2OutOneAString(String input) {
		if (input != null)
			return new String(input);
		return null;
	}

	public String in2OutOneCString(String input) {
		if (input != null)
			return new String(input);
		return null;
	}

	public String in2OutOneDOMString(String input) {
		if (input != null)
			return new String(input);
		return null;
	}

	public int in2OutOneInt(int input) {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public String in2OutOneString(String input) {
		if (input != null)
			return new String(input);
		return null;
	}

	public String in2OutOneUTF8String(String input) {
		if (input != null)
			return new String(input);
		return null;
	}

	public void methodWithForwardDeclaredParam(nsITestXPCSomeUselessThing sut) {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void printArgTypes() {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public nsISupports pseudoQueryInterface(String uuid) {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void returnCode(int code) {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void returnCode_NS_ERROR_NULL_POINTER() {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void returnCode_NS_ERROR_OUT_OF_MEMORY() {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void returnCode_NS_ERROR_UNEXPECTED() {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void returnCode_NS_OK() {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public nsISupports returnInterface(nsISupports obj) {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void sendInOutManyTypes(byte[] p1, short[] p2, int[] p3, long[] p4,
			byte[] p5, int[] p6, long[] p7, double[] p8, float[] p9,
			double[] p10, boolean[] p11, char[] p12, char[] p13, String[] p14,
			String[] p15, String[] p16) {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void sendManyTypes(byte p1, short p2, int p3, long p4, byte p5,
			int p6, long p7, double p8, float p9, double p10, boolean p11,
			char p12, char p13, String p14, String p15, String p16) {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void sendOneString(String str) {
		if (receiver != null) {
			receiver.sendOneString(str);
		}
		results[0] = str;
	}

	public void setAString(String aAString) {
		savedString = aAString;
	}

	public void setReceiver(nsIEcho aReceiver) {
		System.out.println("Setting receiver to nsIEcho impl = " + aReceiver);
		receiver = aReceiver;
	}

	public void setReceiverReturnOldReceiver(nsIEcho[] aReceiver) {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void setSomeValue(int aSomeValue) {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public String sharedString() {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void simpleCallNoEcho() {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	public void throwArg() {
		throw new XPCOMException(Mozilla.NS_ERROR_NOT_IMPLEMENTED);
	}

	/* nsIXPCTestIn */

	public boolean echoBoolean(boolean b) {
		return b;
	}

	public char echoChar(char c) {
		return c;
	}

	public double echoDouble(double d) {
		return d;
	}

	public float echoFloat(float f) {
		return f;
	}

	public int echoLong(int l) {
		return l;
	}

	public long echoLongLong(long ll) {
		return ll;
	}

	public byte echoOctet(byte o) {
		return o;
	}

	public boolean echoPRBool(boolean b) {
		return b;
	}

	public short echoPRInt16(short l) {
		return l;
	}

	public int echoPRInt32(int l) {
		return l;
	}

	public long echoPRInt64(long i) {
		return i;
	}

	public int echoPRUint16(int i) {
		return i;
	}

	public long echoPRUint32(long i) {
		return i;
	}

	public long echoPRUint32_2(long i) {
		return i;
	}

	public double echoPRUint64(double i) {
		return i;
	}

	public short echoPRUint8(short i) {
		return i;
	}

	public short echoShort(short a) {
		return a;
	}

	public String echoString(String ws) {
		return ws;

	}
	public long echoUnsignedLong(long ul) {
		return ul;
	}

	public int echoUnsignedShort(int us) {
		return us;
	}

	public void echoVoid() {
	}

	public char echoWchar(char wc) {
		return wc;
	}

}
