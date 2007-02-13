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

import MultipleExtraArgFunction1.*

function returnRestNoPackage(... rest):Number { return rest.length; }


var SECTION = "Definitions";       // provide a document reference (ie, ECMA section)
var VERSION = "AS3";  // Version of JavaScript or ECMA
var TITLE   = "Optional Argument test";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

var TESTOBJ = new TestObj();
var TESTOBJ1 = new MultipleExtraArgFunction1();

// inside class inside package
AddTestCase( "TESTOBJ.returnRest()", 6, TESTOBJ.returnRest(10,false,"hello",new Object(), [123],[345]) );

// inside package outside of class
AddTestCase( "returnRest()", 7, returnRest([10,11,12],false,"hello",new Object(), [123],[345],"hello") );

// outside package inside class
AddTestCase( "TESTOBJ1.returnRest()", 6, TESTOBJ1.returnRest(10,new Object(),"hello",new Object(), [123],[345]) );

// outside package and outside class
AddTestCase( "returnRestNoPackage()", 6, returnRestNoPackage(10,"str2","hello",new Object(), [123],[345]) );


test();       // leave this alone.  this executes the test cases and
              // displays results.
