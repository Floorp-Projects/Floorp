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
 
 

import PublicClassMethodAndProp.*;

var SECTION = "Definitions";           // provide a document reference (ie, ECMA section)
var VERSION = "AS3";                   // Version of JavaScript or ECMA
var TITLE   = "Access Class Properties & Methods";  // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone



var arr = new Array(1,2,3);

var Obj = new AccPubClassMAndP();

var d = new Date();

var f = new Function();

var str = "Test";

var ob = new Object();


// ********************************************
// access default method
//
// ********************************************

AddTestCase( "*** Access default method of a class ***", 1, 1 );
AddTestCase( "Obj.setArray(arr), Obj.getArray()", arr, Obj.testGetSetArray(arr) );


// ********************************************
// access private method
//
// ********************************************

// TODO: Need to modify the test to only create the date as Date(0) and just check the year
// AddTestCase( "*** Access private method of a class ***", 1, 1 );
// AddTestCase( "Obj.setPrivDate(date), Obj.getPrivDate()", d, Obj.testGetSetPrivDate(d) );


// ********************************************
// access public method
//
// ********************************************

AddTestCase( "*** Access public method of a class ***", 1, 1 );
AddTestCase( "Obj.setPubBoolean(b), Obj.getPubBoolean()", true, (Obj.setPubBoolean(true), Obj.getPubBoolean()) );


// ********************************************
// access static method
//
// ********************************************

AddTestCase( "*** Access static method of a class ***", 1, 1 );
AddTestCase( "Obj.setStatFunction(f), Obj.getStatFunction()", f, Obj.testGetSetStatFunction(f) );

// ********************************************
// access private static method
// ********************************************

AddTestCase( "*** Access private static method of a class ***", 1, 1 );
AddTestCase( "Obj.setPrivStatString(s), Obj.getPrivStatString", str, Obj.testGetSetPrivStatString(str) );


// ********************************************
// access public static method
// ********************************************

AddTestCase( "*** Access public static method of a class ***", 1, 1 );
AddTestCase( "AccPubClassMAndP.setPubStatObject(ob), AccPubClassMAndP.getPubStatObject()", ob,
	(AccPubClassMAndP.setPubStatObject(ob), AccPubClassMAndP.getPubStatObject()) );


// ********************************************
// access final method
// ********************************************

AddTestCase( "*** Access final method of a class ***", 1, 1 );
AddTestCase( "Obj.setFinNumber(10), Obj.getFinNumber()", 10, Obj.testGetSetFinNumber(10) );


// ********************************************
// access public final method
// ********************************************

AddTestCase( "*** Access public final method of a class ***", 1, 1 );
AddTestCase( "Obj.setPubFinArray(arr), Obj.getPubFinArray()", arr, (Obj.setPubFinArray(arr), Obj.getPubFinArray()) );



// ********************************************
// access public property
// ********************************************

AddTestCase( "*** Access public property of a class ***", 1, 1 );
AddTestCase( "Obj.pubBoolean = true, Obj.pubBoolean", true, (Obj.pubBoolean = true, Obj.pubBoolean) );

// ********************************************
// access public static property
// ********************************************

AddTestCase( "*** Access public satic property of a class ***", 1, 1 );
AddTestCase( "AccPubClassMAndP.pubStatObject = ob, AccPubClassMAndP.pubStatObject", ob, (AccPubClassMAndP.pubStatObject = ob, AccPubClassMAndP.pubStatObject) );


// ********************************************
// access public final property
// ********************************************

AddTestCase( "*** Access public final property of a class ***", 1, 1 );
AddTestCase( "Obj.pubFinArray = arr, Obj.pubFinArray", arr, (Obj.pubFinArray = arr, Obj.pubFinArray) );



test();       // leave this alone.  this executes the test cases and
              // displays results.
