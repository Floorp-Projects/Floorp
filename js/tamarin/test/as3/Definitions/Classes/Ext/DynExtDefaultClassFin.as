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



import DefaultClass.*;

var SECTION = "Definitions";       			// provide a document reference (ie, ECMA section)
var VERSION = "AS3";  					// Version of JavaScript or ECMA
var TITLE   = "final Class Extends Default Class";      // Provide ECMA section title or a description
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

var EXTDCLASS = new DynExtDefaultClassFin();
var arr = new Array(10, 15, 20, 25, 30);
  
AddTestCase( "*** Access final method from final method of sub class ***", 1, 1 );
AddTestCase( "EXTDCLASS.testFinSubArray(arr)", arr, (EXTDCLASS.testFinSubArray(arr)));


// ********************************************
// access final method from a default
// method of a sub class
//
// ********************************************

EXTDCLASS = new DynExtDefaultClassFin();
var arr = new Array(1, 2, 3);
AddTestCase( "*** Access final method from default method of sub class ***", 1, 1 );
AddTestCase( "EXTDCLASS.testSubGetSetArray(arr)", arr, (EXTDCLASS.testSubGetSetArray(arr)) );



// ********************************************
// access final method from a public 
// method of a sub class
//
// ********************************************

EXTDCLASS = new DynExtDefaultClassFin();
var arr = new Array(4, 5, 6);
AddTestCase( "*** Access final method from public method of sub class ***", 1, 1 );
AddTestCase( "EXTDCLASS.pubSubSetArray(arr), EXTDCLASS.pubSubGetArray()", arr, (EXTDCLASS.pubSubSetArray(arr), EXTDCLASS.pubSubGetArray()) );


// ********************************************
// access final method from a private 
// method of a sub class
//
// ********************************************

EXTDCLASS = new DynExtDefaultClassFin();
var arr = new Array(10, 50);
AddTestCase( "*** Access final method from private method of sub class ***", 1, 1 );
AddTestCase( "EXTDCLASS.testPrivSubArray(arr)", arr, EXTDCLASS.testPrivSubArray(arr) );


// ********************************************
// access final method from a final 
// method of a sub class
//
// ********************************************

EXTDCLASS = new DynExtDefaultClassFin();
var arr = new Array(4, 5, 6);
AddTestCase( "*** Access final method from public method of sub class ***", 1, 1 );
AddTestCase( "EXTDCLASS.testFinSubArray(arr)", arr, (EXTDCLASS.testFinSubArray(arr)) );



// ********************************************
// access final method from a virtual 
// method of a sub class
// ********************************************

AddTestCase( "access 'final' method from 'virtual' method of sub class", arr, 
			  EXTDCLASS.testVirtSubArray(arr) );

// ********************************************
// access final property from outside
// the class
// ********************************************

EXTDCLASS = new DynExtDefaultClassFin();
AddTestCase( "*** Access final from outside the class ***", 1, 1 );
//AddTestCase( "EXTDCLASS.finArray = arr", arr, (EXTDCLASS.finArray = arr, EXTDCLASS.finArray) );

// ********************************************
// access final property from 
// default method in sub class
// ********************************************

EXTDCLASS = new DynExtDefaultClassFin();
var arr = new Array(10, 20, 30);
AddTestCase( "*** Access final property from default method in sub class ***", 1, 1 );
AddTestCase( "EXTDCLASS.testSubGetSetDPArray(arr)", arr, (EXTDCLASS.testSubGetSetDPArray(arr)) );

// ********************************************
// access final property from 
// public method in sub class
// ********************************************

EXTDCLASS = new DynExtDefaultClassFin();
AddTestCase( "*** Access final property from public method in sub class ***", 1, 1 );
AddTestCase( "EXTDCLASS.pubSubSetDPArray(arr), EXTDCLASS.pubSubGetDPArray()", arr, (EXTDCLASS.pubSubSetDPArray(arr), EXTDCLASS.pubSubGetDPArray()) );


// ********************************************
// access final property from 
// private method in sub class
// ********************************************

EXTDCLASS = new DynExtDefaultClassFin();
AddTestCase( "*** Access final property from private method in sub class ***", 1, 1 );
AddTestCase( "EXTDCLASS.testSubPrivDPArray(arr)", arr, (EXTDCLASS.testSubPrivDPArray(arr)) );

// ********************************************
// access final property from 
// final method in sub class
// ********************************************

EXTDCLASS = new DynExtDefaultClassFin();
AddTestCase( "*** Access final property from final method in sub class ***", 1, 1 );
AddTestCase( "EXTDCLASS.testSubFinDPArray(arr)", arr, (EXTDCLASS.testSubFinDPArray(arr)) );

// ********************************************
// access final property from 
// virtual method in sub class
// ********************************************

DYNEXTDCLASS = new DynExtDefaultClassFin();
AddTestCase( "access 'final' property from 'virtual' method of sub class", arr, 
				(EXTDCLASS.testVirtSubDPArray(arr)) );

test();       // leave this alone.  this executes the test cases and
              // displays results.  
  
