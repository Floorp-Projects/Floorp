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
 
 

import FinalDefaultClassPackage.*;

var SECTION = "Definitions";           // provide a document reference (ie, ECMA section)
var VERSION = "AS3";                   // Version of JavaScript or ECMA
var TITLE   = "Access Class Properties & Methods";  // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone



var arr = new Array(1,2,3);
var arr2 = new Array(3,2,1);
var Obj = new FinalDefaultClassAccessor();
var d = new Date(0);
var d2 = new Date(1);
var f = new Function();
var str = "Test";
var ob = new Object();

// ********************************************
// Access Default method
// ********************************************
AddTestCase( "*** Access default method of a class ***", 1, 1 );
AddTestCase( "Obj.setArray(arr), Obj.getArray()", arr, Obj.testGetSetArray(arr) );

// ********************************************
// Access Default virtual method
// ********************************************
AddTestCase( "*** Access default virtual method of a class ***", 1, 1 );
AddTestCase( "Obj.setVirtualArray(arr2), Obj.getVirtualArray()", arr2, Obj.testGetSetVirtualArray(arr2) );

// ********************************************
// Access Default Static method
// ********************************************
AddTestCase( "*** Access static method of a class ***", 1, 1 );
AddTestCase( "Obj.setStatFunction(f), Obj.getStatFunction()", f, Obj.testGetSetStatFunction(f) );

// ********************************************
// Access Default Final method
// ********************************************
AddTestCase( "*** Access final method of a class ***", 1, 1 );
AddTestCase( "Obj.setFinNumber(10), Obj.getFinNumber()", 10, Obj.testGetSetFinNumber(10) );

// ********************************************
// Access Internal method
// ********************************************
AddTestCase( "*** Access internal method of a class ***", 1, 1 );
AddTestCase( "Obj.setInternalArray(arr), Obj.getInternalArray()", arr, Obj.testGetSetInternalArray(arr) );

// ********************************************
// Access Internal virtual method
// ********************************************
AddTestCase( "*** Access internal virtual method of a class ***", 1, 1 );
AddTestCase( "Obj.setInternalVirtualArray(arr2), Obj.getInternalVirtualArray()", arr2, Obj.testGetSetInternalVirtualArray(arr2) );

// ********************************************
// Access Internal Static method
// ********************************************
AddTestCase( "*** Access internal static method of a class ***", 1, 1 );
AddTestCase( "Obj.setInternalStatFunction(f), Obj.getInternalStatFunction()", f, Obj.testGetSetInternalStatFunction(f) );

// ********************************************
// Access Internal Final method
// ********************************************
AddTestCase( "*** Access internal final method of a class ***", 1, 1 );
AddTestCase( "Obj.setInternalFinNumber(10), Obj.getInternalFinNumber()", 10, Obj.testGetSetInternalFinNumber(10) );

// ********************************************
// Access Private method
// ********************************************
AddTestCase( "*** Access private method of a class ***", 1, 1 );
AddTestCase( "Obj.setPrivDate(date), Obj.getPrivDate()", d.getFullYear(), Obj.testGetSetPrivDate(d).getFullYear() );

// ********************************************
// Access Private virtualmethod
// ********************************************
AddTestCase( "*** Access private virtual method of a class ***", 1, 1 );
AddTestCase( "Obj.setPrivVirtualDate(date2), Obj.getPrivVirtaulDate()", d2.getFullYear(), Obj.testGetSetPrivVirtualDate(d2).getFullYear() );

// ********************************************
// Access Private Static method
// ********************************************
AddTestCase( "*** Access private static method of a class ***", 1, 1 );
AddTestCase( "Obj.setPrivStatString(s), Obj.getPrivStatString", str, Obj.testGetSetPrivStatString(str) );

// ********************************************
// Access Private Final method
// ********************************************
AddTestCase( "*** Access private final method of a class ***", 1, 1 );
AddTestCase( "Obj.setPrivFinalString(s), Obj.getPrivFinalString", str, Obj.testGetSetPrivFinalString(str) );

// ********************************************
// Access Public method
// ********************************************
AddTestCase( "*** Access public method of a class ***", 1, 1 );
AddTestCase( "Obj.setPubBoolean(b), Obj.getPubBoolean()", true, (Obj.setPubBoolean(true), Obj.getPubBoolean()) );

// ********************************************
// Access Public virtual method
// ********************************************
AddTestCase( "*** Access public virtual method of a class ***", 1, 1 );
AddTestCase( "Obj.setPubVirtualBoolean(false), Obj.getPubVirtualBoolean()", false, (Obj.setPubVirtualBoolean(false), Obj.getPubVirtualBoolean()) );

// ********************************************
// Access Public Static method
// ********************************************
AddTestCase( "*** Access public static method of a class ***", 1, 1 );
AddTestCase( "Obj..setPubStatObject(ob), Obj.getPubStatObject()", ob, (Obj.setPubStatObject(ob), Obj.getPubStatObject()) );

// ********************************************
// Access Public Final method
// ********************************************
AddTestCase( "*** Access public final method of a class ***", 1, 1 );
AddTestCase( "Obj.setPubFinArray(arr), Obj.getPubFinArray()", arr, (Obj.setPubFinArray(arr), Obj.getPubFinArray()) );



test();       // leave this alone.  this executes the test cases and
              // displays results.
