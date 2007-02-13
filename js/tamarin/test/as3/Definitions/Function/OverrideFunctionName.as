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
import OverrideFunctionName.*;

// inside class outside package
class OverrideFunctionNameClassBase {
    	// constructor
    	function OverrideFunctionNameClassBase() {}

    	// not the constructor but looks like it
    	function overrideFunctionNameClass() { return null; }

    	function a1 () { return null; }
    	function a_1 () { return null; }
    	function _a1 () { return null; }
    	function __a1 () { return null; }
    	function _a1_ () { return null; }
    	function __a1__ () { return null; }
    	function $a1 () { return null; }
    	function a$1 () { return null; }
    	function a1$ () { return null; }
    	function A1 () { return null; }
    	function cases () { return null; }
    	function Cases () { return null; }
    	function abcdefghijklmnopqrstuvwxyz0123456789$_ () { return null; }
}

class OverrideFunctionNameClass extends OverrideFunctionNameClassBase {
    	// constructor
    	function OverrideFunctionNameClass() {}

    	// not the constructor but looks like it
    	override function overrideFunctionNameClass() { return "not the constructor" }

    	override function a1 () { return "a1"; }
    	override function a_1 () { return "a_1"; }
    	override function _a1 () { return "_a1"; }
    	override function __a1 () { return "__a1"; }
    	override function _a1_ () { return "_a1_"; }
    	override function __a1__ () { return "__a1__"; }
    	override function $a1 () { return "$a1"; }
    	override function a$1 () { return "a$1"; }
    	override function a1$ () { return "a1$"; }
    	override function A1 () { return "A1"; }
    	override function cases () { return "cases"; }
    	override function Cases () { return "Cases"; }
    	override function abcdefghijklmnopqrstuvwxyz0123456789$_ () { return "all"; }
}


var SECTION = "Definitions";       // provide a document reference (ie, ECMA section)
var VERSION = "AS3";  // Version of JavaScript or ECMA
var TITLE   = "Function Names";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

var TESTOBJ;

// inside class inside package
TESTOBJ = new TestNameObj();
AddTestCase( "inside class inside package function Name a1()", "a1", TESTOBJ.a1() );
AddTestCase( "inside class inside package function Name a_1()", "a_1", TESTOBJ.a_1() );
AddTestCase( "inside class inside package function Name _a1()", "_a1", TESTOBJ._a1() );
AddTestCase( "inside class inside package function Name __a1()", "__a1", TESTOBJ.__a1() );
AddTestCase( "inside class inside package function Name _a1_()", "_a1_", TESTOBJ._a1_() );
AddTestCase( "inside class inside package function Name __a1__()", "__a1__", TESTOBJ.__a1__() );
AddTestCase( "inside class inside package function Name $a1()", "$a1", TESTOBJ.$a1() );
AddTestCase( "inside class inside package function Name a$1()", "a$1", TESTOBJ.a$1() );
AddTestCase( "inside class inside package function Name a1$()", "a1$", TESTOBJ.a1$() );
AddTestCase( "inside class inside package function Name A1()", "A1", TESTOBJ.A1() );
AddTestCase( "inside class inside package function Name cases()", "cases", TESTOBJ.cases() );
AddTestCase( "inside class inside package function Name Cases()", "Cases", TESTOBJ.Cases() );
AddTestCase( "inside class inside package function Name all()", "all", TESTOBJ.abcdefghijklmnopqrstuvwxyz0123456789$_() );
AddTestCase( "inside class inside package function Name constructor different case", "not the constructor", TESTOBJ.testNameObj() );

// inside class outside package
TESTOBJ = new OverrideFunctionNameClass();
AddTestCase( "inside class outside package function Name a1()", "a1", TESTOBJ.a1() );
AddTestCase( "inside class outside package function Name a_1()", "a_1", TESTOBJ.a_1() );
AddTestCase( "inside class outside package function Name _a1()", "_a1", TESTOBJ._a1() );
AddTestCase( "inside class outside package function Name __a1()", "__a1", TESTOBJ.__a1() );
AddTestCase( "inside class outside package function Name _a1_()", "_a1_", TESTOBJ._a1_() );
AddTestCase( "inside class outside package function Name __a1__()", "__a1__", TESTOBJ.__a1__() );
AddTestCase( "inside class outside package function Name $a1()", "$a1", TESTOBJ.$a1() );
AddTestCase( "inside class outside package function Name a$1()", "a$1", TESTOBJ.a$1() );
AddTestCase( "inside class outside package function Name a1$()", "a1$", TESTOBJ.a1$() );
AddTestCase( "inside class outside package function Name A1()", "A1", TESTOBJ.A1() );
AddTestCase( "inside class outside package function Name cases()", "cases", TESTOBJ.cases() );
AddTestCase( "inside class outside package function Name Cases()", "Cases", TESTOBJ.Cases() );
AddTestCase( "inside class outside package function Name all()", "all", TESTOBJ.abcdefghijklmnopqrstuvwxyz0123456789$_() );
AddTestCase( "inside class outside package function Name constructor different case", "not the constructor", TESTOBJ.overrideFunctionNameClass() );


test();       // leave this alone.  this executes the test cases and
              // displays results.
