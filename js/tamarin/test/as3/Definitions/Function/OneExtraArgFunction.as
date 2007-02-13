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
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
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

import OneExtraArgFunction.*

function returnRestNoPackage(... rest):Number { return rest.length; }


var SECTION = "Definitions";       // provide a document reference (ie, ECMA section)
var VERSION = "AS3";  // Version of JavaScript or ECMA
var TITLE   = "Optional Argument test";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

var TESTOBJ = new TestObj();
var TESTOBJ1 = new OneExtraArgFunctionClass();

// Pass a Number argument
// inside class inside package
AddTestCase( "Number TESTOBJ.returnRest()", 1, TESTOBJ.returnRest(10) );

// inside package outside of class
AddTestCase( "Number returnRest()", 1, returnRest(20) );

// outside package inside class
AddTestCase( "Number TESTOBJ1.returnRest()", 1, TESTOBJ1.returnRest(30) );

// outside package and outside class
AddTestCase( "Number returnRestNoPackage()", 1, returnRestNoPackage(40) );


// Pass a String argument
// inside class inside package
AddTestCase( "String TESTOBJ.returnRest()", 1, TESTOBJ.returnRest("Str1") );

// inside package outside of class
AddTestCase( "String returnRest()", 1, returnRest("Str2") );

// outside package inside class
AddTestCase( "String TESTOBJ1.returnRest()", 1, TESTOBJ1.returnRest("Str3") );

// outside package and outside class
AddTestCase( "String returnRestNoPackage()", 1, returnRestNoPackage("Str4") );


// Pass a Boolean argument
// inside class inside package
AddTestCase( "Boolean TESTOBJ.returnRest()", 1, TESTOBJ.returnRest(true) );

// inside package outside of class
AddTestCase( "Boolean returnRest()", 1, returnRest(false) );

// outside package inside class
AddTestCase( "Boolean TESTOBJ1.returnRest()", 1, TESTOBJ1.returnRest(true) );

// outside package and outside class
AddTestCase( "Boolean returnRestNoPackage()", 1, returnRestNoPackage(false) );


// Pass an Object argument
// inside class inside package
AddTestCase( "Object TESTOBJ.returnRest()", 1, TESTOBJ.returnRest(new Object()) );

// inside package outside of class
AddTestCase( "Object returnRest()", 1, returnRest(new Object()) );

// outside package inside class
AddTestCase( "Object TESTOBJ1.returnRest()", 1, TESTOBJ1.returnRest(new Object()) );

// outside package and outside class
AddTestCase( "Object returnRestNoPackage()", 1, returnRestNoPackage(new Object()) );

// Pass an Array argument
// inside class inside package
AddTestCase( "Array TESTOBJ.returnRest()", 1, TESTOBJ.returnRest([10,20,30]) );

// inside package outside of class
AddTestCase( "Array returnRest()", 1, returnRest([10,20,30]) );

// outside package inside class
AddTestCase( "Array TESTOBJ1.returnRest()", 1, TESTOBJ1.returnRest([10,20,30]) );

// outside package and outside class
AddTestCase( "Array returnRestNoPackage()", 1, returnRestNoPackage([10,20,30]) );


test();       // leave this alone.  this executes the test cases and
              // displays results.
