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


/**
 *  All 'import' statements  should be the first 
 *  in a file.
 */
import PublicClass.*;

var SECTION = "Definitions";       			// provide a document reference (ie, ECMA section)
var VERSION = "AS 3.0";  				// Version of JavaScript or ECMA
var TITLE   = "dynamic Class Extends Public Class";    	// Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                				// leave this alone

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

  
//*******************************************
//  access final method from 
//  outside of class
//*******************************************

var DYNEXTDCLASS = new DynExtPublicClassFin();
var arr = new Array(1, 2, 3);


// ********************************************
// access final method from a default
// method of a sub class
//
// ********************************************

DYNEXTDCLASS = new DynExtPublicClassFin();
AddTestCase( "*** Access final method from default method of sub class ***", 1, 1 );
AddTestCase( "DYNEXTDCLASS.testSubArray(arr)", arr, (DYNEXTDCLASS.testSubArray(arr)) );



// ********************************************
// access final method from a public 
// method of a sub class
//
// ********************************************

DYNEXTDCLASS = new DynExtPublicClassFin();
AddTestCase( "*** Access final method from public method of sub class ***", 1, 1 );
AddTestCase( "DYNEXTDCLASS.pubSubSetArray(arr), DYNEXTDCLASS.pubSubGetArray()", arr, (DYNEXTDCLASS.pubSubSetArray(arr), DYNEXTDCLASS.pubSubGetArray()) );


// ********************************************
// access final method from a private 
// method of a sub class
//
// ********************************************

DYNEXTDCLASS = new DynExtPublicClassFin();
AddTestCase( "*** Access final method from private method of sub class ***", 1, 1 );
AddTestCase( "DYNEXTDCLASS.testPrivSubArray(arr)", arr, DYNEXTDCLASS.testPrivSubArray(arr) );


// ********************************************
// access final method from a final 
// method of a sub class
//
// ********************************************

DYNEXTDCLASS = new DynExtPublicClassFin();
AddTestCase( "*** Access final method from final method of sub class ***", 1, 1 );
AddTestCase( "DYNEXTDCLASS.testFinSubArray(arr)", arr, (DYNEXTDCLASS.testFinSubArray(arr)) );


// ********************************************
// access final method from a public final 
// method of a sub class
//
// ********************************************

DYNEXTDCLASS = new DynExtPublicClassFin();
AddTestCase( "*** Access final method from public final method of sub class ***", 1, 1 );
AddTestCase( "DYNEXTDCLASS.pubFinSubSetArray(arr), DYNEXTDCLASS.pubFinSubGetArray()", arr, (DYNEXTDCLASS.pubFinSubSetArray(arr), DYNEXTDCLASS.pubFinSubGetArray()) );

// ********************************************
// access final method from a private final 
// method of a sub class
//
// ********************************************

DYNEXTDCLASS = new DynExtPublicClassFin();
AddTestCase( "*** Access final method from private final method of sub class ***", 1, 1 );
AddTestCase( "DYNEXTDCLASS.testPrivFinSubArray(arr)", arr, (DYNEXTDCLASS.testPrivFinSubArray(arr)) );



// ********************************************
// access final property from 
// default method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtPublicClassFin();
AddTestCase( "*** Access final property from method in sub class ***", 1, 1 );
AddTestCase( "DYNEXTDCLASS.testSubDPArray(arr)", arr, (DYNEXTDCLASS.testSubDPArray(arr)) );


// ********************************************
// access final property from 
// public method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtPublicClassFin();
AddTestCase( "*** Access final property from public method in sub class ***", 1, 1 );
AddTestCase( "DYNEXTDCLASS.pubSubSetDPArray(arr), DYNEXTDCLASS.pubSubGetDPArray()", arr, (DYNEXTDCLASS.pubSubSetDPArray(arr), DYNEXTDCLASS.pubSubGetDPArray()) );


// ********************************************
// access final property from 
// private method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtPublicClassFin();
AddTestCase( "*** Access final property from private method in sub class ***", 1, 1 );
AddTestCase( "DYNEXTDCLASS.testPrivSubDPArray(arr)", arr, (DYNEXTDCLASS.testPrivSubDPArray(arr)) );



// ********************************************
// access final property from 
// final method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtPublicClassFin();
AddTestCase( "*** Access final property from final method in sub class ***", 1, 1 );
AddTestCase( "DYNEXTDCLASS.testFinSubDPArray(arr)", arr, (DYNEXTDCLASS.testFinSubDPArray(arr)) );

// ********************************************
// access final property from 
// virtual method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtPublicClassFin();
AddTestCase( "*** Access final property from virtual method in sub class ***", 1, 1 );
AddTestCase( "DYNEXTDCLASS.testVirSubDPArray(arr)", arr, (DYNEXTDCLASS.testVirSubDPArray(arr)) );

// ********************************************
// access final property from 
// private virtual method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtPublicClassFin();
AddTestCase( "*** Access final property from private virtual method in sub class ***", 1, 1 );
AddTestCase( "DYNEXTDCLASS.testPrivVirSubDPArray(arr)", arr, (DYNEXTDCLASS.testVirSubDPArray(arr)) );


test();       		// Leave this function alone.
			// This function is for executing the test case and then 
			// displaying the result on to the console or the LOG file.
