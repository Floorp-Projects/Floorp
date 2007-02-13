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
 
 

import DynamicClassPropPackage.*;

var SECTION = "Definitions";           // provide a document reference (ie, ECMA section)
var VERSION = "AS3";                   // Version of JavaScript or ECMA
var TITLE   = "Add Properties to Dynamic Class";  // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone



var Obj = new DynamicClassProp();


var arr = new Array(1,2,3);

var d = new Date(0);

var str = "Test";

var ob = new Object();


Obj.prop1 = 100;
Obj.prop2 = "Test";
Obj.prop3 = true;
Obj.prop4 = arr;
Obj.prop5 = d;
Obj.prop6 = ob;


// ********************************************
// Access property of type Number
//
// ********************************************

AddTestCase( "*** Access property of type Number ***", 1, 1 );
AddTestCase( "Obj.prop1", 100, Obj.prop1 );


// ********************************************
// Access property of type String
//
// ********************************************

AddTestCase( "*** Access property of type String ***", 1, 1 );
AddTestCase( "Obj.prop2", "Test", Obj.prop2 );


// ********************************************
// Access property of type Boolean
//
// ********************************************

AddTestCase( "*** Access property of type Boolean ***", 1, 1 );
AddTestCase( "Obj.prop3", true, Obj.prop3 );


// ********************************************
// Access property of type Array
//
// ********************************************

AddTestCase( "*** Access property of type Array ***", 1, 1 );
AddTestCase( "Obj.prop4", arr, Obj.prop4 );

// ********************************************
// Access property of type Date
// ********************************************

AddTestCase( "*** Access property of type Date ***", 1, 1 );
AddTestCase( "Obj.prop5", d, Obj.prop5 );


// ********************************************
// Access property of type Object
// ********************************************

AddTestCase( "*** Access property of type Object ***", 1, 1 );
AddTestCase( "Obj.prop6", ob, Obj.prop6 );



test();       // leave this alone.  this executes the test cases and
              // displays results.
