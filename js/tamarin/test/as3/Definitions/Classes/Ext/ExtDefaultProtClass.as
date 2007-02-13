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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2004-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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


import DefaultProtClass.*;

var SECTION = "Definitions";       // provide a document reference (ie, ECMA section)
var VERSION = "AS3";  // Version of JavaScript or ECMA
var TITLE   = "Extend Default Class With Protected Variables/Methods";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

/**
 * Calls to AddTestCase here. AddTestCase is a function that is defined
 * in shell.js and takes three arguments:
 * - a string representation of what is being tested
 * - the expected result
 * - the actual result
 *
 * For example, a test might look like this:
 *
 * var helloWorld = "Hello World";
 *
 * AddTestCase(
 * "var helloWorld = 'Hello World'",   // description of the test
 *  "Hello World",                     // expected result
 *  helloWorld );                      // actual result
 *
 */


// ********************************************
// access protected method from a default
// method of a sub class
//
// ********************************************

var arr = new Array(1,2,3);
EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' method from 'default' method of sub class", arr, (EXTDCLASS.testSubGetSetArray(arr)) );


// ********************************************
// access protected method from a public
// method of a sub class
// ********************************************

arr = new Array( 4, 5, 6 );
EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' method from 'public' method of sub class", arr,
			 (EXTDCLASS.pubSubSetArray(arr), EXTDCLASS.pubSubGetArray()) );

// ********************************************
// access protected method from a final
// method of a sub class
// ********************************************

arr = new Array( "one", "two", "three" );
EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' method from 'final' method of sub class", arr, (EXTDCLASS.testFinSubArray(arr)) );

// ********************************************
// access protected method from a public
// final method of a sub class
// ********************************************

arr = new Array( 8, "two", 9 );
EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' method from 'public final' method of sub class", arr,
		     (EXTDCLASS.pubFinSubSetArray(arr), EXTDCLASS.pubFinSubGetArray()) );

// ********************************************
// access protected method from a final
// private method of a sub class
// ********************************************

arr = new Array( "one", "two", "three" );
EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' method from 'private final' method of sub class", arr, (EXTDCLASS.testPrivFinSubArray(arr)) );

// ********************************************
// access protected method from a private
// method of a sub class
// ********************************************

arr = new Array( 5, 6, 7 );
EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' method from 'private' method of sub class", arr, EXTDCLASS.testPrivSubArray(arr) );

// ********************************************
// access protected method from a virtual
// method of a sub class
// ********************************************

AddTestCase( "access 'protected' method from 'virtual' method of sub class", arr,
			  EXTDCLASS.testVirtSubArray(arr) );

// ********************************************
// access protected method from a virtual
// public method of a sub class
// ********************************************

AddTestCase( "access 'protected' method from 'public virtual' method of sub class", arr,
		     (EXTDCLASS.pubVirtSubSetArray(arr), EXTDCLASS.pubVirtSubGetArray()) );

// ********************************************
// access protected method from a virtual
// private method of a sub class
// ********************************************

AddTestCase( "access 'protected' method from 'private virtual' method of sub class", arr,
			  EXTDCLASS.testPrivVirtSubArray(arr) );

// ********************************************
// access protected method from static
// method of sub class
// ********************************************

var thisError = "no exception thrown";
try{
	ExtDefaultProtClass.pubStatSubGetArray();
} catch (e1) {
	thisError = e1.toString();
} finally {
	AddTestCase( "access 'protected' method from 'static' method of the sub class",
				TYPEERROR+1006,
				typeError( thisError) );
}

// ********************************************
// access protected property from
// default method in sub class
// ********************************************

EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' property from 'default' method of sub class", arr,
				(EXTDCLASS.testSubGetSetDPArray(arr)) );

// ********************************************
// access protected property from
// final method in sub class
// ********************************************

EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' property from 'final' method of sub class", arr,
				(EXTDCLASS.testFinSubDPArray(arr)) );

// ********************************************
// access protected property from
// virtual method in sub class
// ********************************************

EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' property from 'virtual' method of sub class", arr,
				(EXTDCLASS.testVirtSubDPArray(arr)) );

// ********************************************
// access protected property from
// public method in sub class
// ********************************************

EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' property from 'public' method of sub class", arr,
				(EXTDCLASS.pubSubSetDPArray(arr), EXTDCLASS.pubSubGetDPArray()) );

// ********************************************
// access protected property from
// private method in sub class
// ********************************************

EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' property from 'private' method of sub class", arr,
			 (EXTDCLASS.testPrivSubDPArray(arr)) );

// ********************************************
// access protected property from
// public final method in sub class
// ********************************************

EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' property from 'public final' method of sub class", arr,
			 (EXTDCLASS.pubFinSubSetDPArray(arr), EXTDCLASS.pubFinSubGetDPArray()) );

// ********************************************
// access protected property from
// public virtual method in sub class
// ********************************************

EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' property from 'public virtual' method of sub class", arr,
			 (EXTDCLASS.pubVirtSubSetDPArray(arr), EXTDCLASS.pubVirtSubGetDPArray()) );

// ********************************************
// access protected property from
// private final method in sub class
// ********************************************

EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' property from 'private final' method of sub class", arr,
			 (EXTDCLASS.testPrivFinSubDPArray(arr)) );

// ********************************************
// access protected property from
// private virtual method in sub class
// ********************************************

EXTDCLASS = new ExtDefaultProtClass();
AddTestCase( "access 'protected' property from 'private virtual' method of sub class", arr,
			 (EXTDCLASS.testPrivVirtSubDPArray(arr)) );

// ********************************************
// access protected property from
// static public method of sub class
// ********************************************

thisError = "no error thrown";
try{
	ExtDefaultProtClass.pubStatSubGetDPArray();
} catch(e3) {
	thisError = e3.toString();
} finally {
	AddTestCase( "access protected property from static public method of sub class",
			TYPEERROR+1006,
			typeError( thisError ) );
}

test();       // leave this alone.  this executes the test cases and
              // displays results.
