/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

/**
 *	Get and set properties on a XPCOM object and verify that they are
 *  JavaScripty.
 *
 */

StartTest( "Get and Set Properties on a native object" );
SetupTest();
AddTestData();
StopTest();

function SetupTest() {
	CONTRACTID = "@mozilla.org/js/xpc/test/Overloaded;1";
	CLASS = Components.classes[CONTRACTID].createInstance();
	IFACE = Components.interfaces.nsIXPCTestScriptable;

	testObject = CLASS.QueryInterface(IFACE);
}

function AddTestData() {
	// add a property to the object.

	testObject.newProperty = "PASS",
	AddTestCase(
	"testObject.newProperty = \"PASS\"; testObject.newProperty",
	"PASS",
	testObject.newProperty );

	// delete a property from an object
	var result = delete testObject.newProperty;
	
	AddTestCase(
		"delete testObject.newProperty",
		true,
		result );
	AddTestCase(
		"delete testObject.newProperty; testObject.newProperty",
		undefined,
		testObject.newProperty );

	// add a function

	testObject.newFunction = new Function( "return \"PASSED\"" );
	
	AddTestCase(
		"testObject.newFunction = new Function(\"return 'PASSED'\"); " +
		"typeof testObject.newFunction",
		"function",
		typeof testObject.newFunction );

	var s = "testObject.newFunction()"

	AddTestCase(
		"testObject.newFunction()",
		"PASSED",
		eval(s));

	// try to call the function as a constructor
	testObject.newFunction = new Function( "this.result = \"PASSED\"" );

	AddTestCase( 
		"testObject.newFunction = new Function( 'this.result = \"PASSED\"'); "+
		"new testObject.newFunction().result",
		"PASSED",
		new testObject.newFunction().result );


	// delete the function

	result = delete testObject.newFunction;
	AddTestCase(
		"delete testObject.newFunction",
		true,
		result);

	AddTestCase(
		"typeof testObject.newFunction",
		"undefined",
		typeof testObject.newFunction);

}
