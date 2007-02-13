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

var SECTION = "Definitions";       // provide a document reference (ie, ECMA section)
var VERSION = "Clean AS2";  // Version of JavaScript or ECMA
var TITLE   = "Extend Default Class";       // Provide ECMA section title or a description
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


var EXTDCLASS = new ExtDefaultClassPub();
  
//*******************************************
// public Methods and public properties
//
// call a public Method of an object that 
// inherited it from it's parent class
//*******************************************

arr = new Array(1, 2, 3);
date = new Date(0);
func = new Function();
num = new Number();
obj = new Object();
str = new String("test");
//sim = new Simple();
//math = new Math();

AddTestCase( "*** Public Methods and Public properites ***", 1, 1 );
AddTestCase( "EXTDCLASS.setPubArray(arr), EXTDCLASS.pubArray", arr, (EXTDCLASS.setPubArray(arr), EXTDCLASS.pubArray) );
AddTestCase( "EXTDCLASS.setPubBoolean(true), EXTDCLASS.pubBoolean", true, (EXTDCLASS.setPubBoolean(true), EXTDCLASS.pubBoolean) );
//AddTestCase( "EXTDCLASS.setPubDate(date), EXTDCLASS.pubDate", date, (EXTDCLASS.setPubDate(date), EXTDCLASS.pubDate) );
AddTestCase( "EXTDCLASS.setPubFunction(func), EXTDCLASS.pubFunction", func, (EXTDCLASS.setPubFunction(func), EXTDCLASS.pubFunction) );
//AddTestCase( "EXTDCLASS.setPubMath(math), EXTDCLASS.pubMath", math, (EXTDCLASS.setPubMath(math), EXTDCLASS.pubMath) );
AddTestCase( "EXTDCLASS.setPubNumber(num), EXTDCLASS.pubNumber", num, (EXTDCLASS.setPubNumber(num), EXTDCLASS.pubNumber) );
AddTestCase( "EXTDCLASS.setPubObject(obj), EXTDCLASS.pubObject", obj, (EXTDCLASS.setPubObject(obj), EXTDCLASS.pubObject) );
AddTestCase( "EXTDCLASS.setPubString(str), EXTDCLASS.pubString", str, (EXTDCLASS.setPubString(str), EXTDCLASS.pubString) );
//AddTestCase( "EXTDCLASS.setPubSimple(sim), EXTDCLASS.pubSimple", sim, (EXTDCLASS.setPubSimple(sim), EXTDCLASS.pubSimple) );


// ********************************************
// access public method from a default
// method of a sub class
//
// ********************************************

EXTDCLASS = new ExtDefaultClassPub();
AddTestCase( "EXTDCLASS.testSubArray(arr)", arr, EXTDCLASS.testSubArray(arr) );


// ********************************************
// access public method from a public 
// method of a sub class
//
// ********************************************

EXTDCLASS = new ExtDefaultClassPub();
AddTestCase( "EXTDCLASS.pubSubSetArray(arr), EXTDCLASS.pubSubGetArray()", arr, (EXTDCLASS.pubSubSetArray(arr), EXTDCLASS.pubSubGetArray()) );


// ********************************************
// access public method from a private 
// method of a sub class
//
// ********************************************

EXTDCLASS = new ExtDefaultClassPub();
AddTestCase( "EXTDCLASS.testPrivSubArray(arr)", arr, EXTDCLASS.testPrivSubArray(arr) );

// ********************************************
// access public property from outside
// the class
// ********************************************

EXTDCLASS = new ExtDefaultClassPub();
AddTestCase( "EXTDCLASS.pubArray = arr", arr, (EXTDCLASS.pubArray = arr, EXTDCLASS.pubArray) );


// ********************************************
// access public property from 
// default method in sub class
// ********************************************

EXTDCLASS = new ExtDefaultClassPub();
AddTestCase( "EXTDCLASS.testSubDPArray(arr)", arr, EXTDCLASS.testSubDPArray(arr) );


// ********************************************
// access public property from 
// public method in sub class
// ********************************************

EXTDCLASS = new ExtDefaultClassPub();
AddTestCase( "EXTDCLASS.pubSubSetDPArray(arr), EXTDCLASS.pubSubGetDPArray()", arr, (EXTDCLASS.pubSubSetDPArray(arr), EXTDCLASS.pubSubGetDPArray()) );


// ********************************************
// access public property from 
// private method in sub class
// ********************************************

EXTDCLASS = new ExtDefaultClassPub();
AddTestCase( "EXTDCLASS.testPrivSubPArray(arr)", arr, EXTDCLASS.testPrivSubPArray(arr) );

test();       // leave this alone.  this executes the test cases and
              // displays results.
