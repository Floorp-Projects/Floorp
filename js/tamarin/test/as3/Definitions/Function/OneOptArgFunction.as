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

import OneOptArgFunction.*

function returnStringNoPackage(s:String = "outside package and outside class",... rest):String { return s; }
function returnBooleanNoPackage(b:Boolean = true,... rest):Boolean { return b; }
function returnNumberNoPackage(n:Number = 10,... rest):Number { return n; }

var SECTION = "Definitions";       // provide a document reference (ie, ECMA section)
var VERSION = "AS3";  // Version of JavaScript or ECMA
var TITLE   = "Function Body Parameter/Result Type";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

var TESTOBJ = new TestObj();
var TESTOBJ1 = new OneOptArgFunctionClass();
var s:String = new String("this is a test");
var b:Boolean = new Boolean(true);

//Optional String argument
// inside class inside package
AddTestCase( "TESTOBJ.returnString()", "inside class inside package", TESTOBJ.returnString() );

// inside package outside of class
AddTestCase( "returnString()", "inside package outside of class", returnString() );

// outside package inside class
AddTestCase( "TESTOBJ1.returnString()", "outside package inside class", TESTOBJ1.returnString() );

// outside package and outside class
AddTestCase( "returnStringNoPackage()", "outside package and outside class", returnStringNoPackage("outside package and outside class",true) );



//Optional Boolean argument
// inside class inside package
AddTestCase( "TESTOBJ.returnBoolean()", true, TESTOBJ.returnBoolean() );

// inside package outside of class
AddTestCase( "returnBoolean()", true, returnBoolean() );

// outside package inside class
AddTestCase( "TESTOBJ1.returnBoolean()", true, TESTOBJ1.returnBoolean() );

// outside package and outside class
AddTestCase( "returnBooleanNoPackage()", true, returnBooleanNoPackage() );


//Optional Number argument
// inside class inside package
AddTestCase( "TESTOBJ.returnNumber()", 10, TESTOBJ.returnNumber() );

// inside package outside of class
AddTestCase( "returnNumber()", 12, returnNumber() );

// outside package inside class
AddTestCase( "TESTOBJ1.returnNumber()", 9, TESTOBJ1.returnNumber(9,10) );

// outside package and outside class
AddTestCase( "returnNumberNoPackage()", 11, returnNumberNoPackage(11,"Hello") );


test();       // leave this alone.  this executes the test cases and
              // displays results.
