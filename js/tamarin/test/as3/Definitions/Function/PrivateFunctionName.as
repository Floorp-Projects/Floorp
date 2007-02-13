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

import PrivateFunctionName.*;

class PrivateFunctionNameClass {

   	private function a1 () { return "a1"; }
	private function a_1 () { return "a_1"; }
	private function _a1 () { return "_a1"; }
	private function __a1 () { return "__a1"; }
	private function _a1_ () { return "_a1_"; }
	private function __a1__ () { return "__a1__"; }
	private function $a1 () { return "$a1"; }
	private function a$1 () { return "a$1"; }
	private function a1$ () { return "a1$"; }
	private function A1 () { return "A1"; }
	private function cases () { return "cases"; }
	private function Cases () { return "Cases"; }
	private function abcdefghijklmnopqrstuvwxyz0123456789$_ () { return "all"; }

	public function puba1 () { return a1(); }
	public function puba_1 () { return a_1(); }
	public function pub_a1 () { return _a1(); }
	public function pub__a1 () { return __a1(); }
	public function pub_a1_ () { return _a1_(); }
	public function pub__a1__ () { return __a1__(); }
	public function pub$a1 () { return $a1(); }
	public function puba$1 () { return a$1(); }
	public function puba1$ () { return a1$(); }
	public function pubA1 () { return A1(); }
	public function pubcases () { return cases(); }
	public function pubCases () { return Cases(); }
	public function puball () { return abcdefghijklmnopqrstuvwxyz0123456789$_(); }
}

var SECTION = "Definitions";       // provide a document reference (ie, ECMA section)
var VERSION = "AS3";  // Version of JavaScript or ECMA
var TITLE   = "Function Names";       // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

var TESTOBJ;

// inside class inside package
TESTOBJ = new TestNameObj();
AddTestCase( "inside class inside package function Name a1()", "a1", TESTOBJ.puba1() );
AddTestCase( "inside class inside package function Name a_1()", "a_1", TESTOBJ.puba_1() );
AddTestCase( "inside class inside package function Name _a1()", "_a1", TESTOBJ.pub_a1() );
AddTestCase( "inside class inside package function Name __a1()", "__a1", TESTOBJ.pub__a1() );
AddTestCase( "inside class inside package function Name _a1_()", "_a1_", TESTOBJ.pub_a1_() );
AddTestCase( "inside class inside package function Name __a1__()", "__a1__", TESTOBJ.pub__a1__() );
AddTestCase( "inside class inside package function Name $a1()", "$a1", TESTOBJ.pub$a1() );
AddTestCase( "inside class inside package function Name a$1()", "a$1", TESTOBJ.puba$1() );
AddTestCase( "inside class inside package function Name a1$()", "a1$", TESTOBJ.puba1$() );
AddTestCase( "inside class inside package function Name A1()", "A1", TESTOBJ.pubA1() );
AddTestCase( "inside class inside package function Name cases()", "cases", TESTOBJ.pubcases() );
AddTestCase( "inside class inside package function Name Cases()", "Cases", TESTOBJ.pubCases() );
AddTestCase( "inside class inside package function Name all()", "all", TESTOBJ.puball() );

// inside class outside package
TESTOBJ = new PrivateFunctionNameClass();
AddTestCase( "inside class outside package function Name a1()", "a1", TESTOBJ.puba1() );
AddTestCase( "inside class outside package function Name a_1()", "a_1", TESTOBJ.puba_1() );
AddTestCase( "inside class outside package function Name _a1()", "_a1", TESTOBJ.pub_a1() );
AddTestCase( "inside class outside package function Name __a1()", "__a1", TESTOBJ.pub__a1() );
AddTestCase( "inside class outside package function Name _a1_()", "_a1_", TESTOBJ.pub_a1_() );
AddTestCase( "inside class outside package function Name __a1__()", "__a1__", TESTOBJ.pub__a1__() );
AddTestCase( "inside class outside package function Name $a1()", "$a1", TESTOBJ.pub$a1() );
AddTestCase( "inside class outside package function Name a$1()", "a$1", TESTOBJ.puba$1() );
AddTestCase( "inside class outside package function Name a1$()", "a1$", TESTOBJ.puba1$() );
AddTestCase( "inside class outside package function Name A1()", "A1", TESTOBJ.pubA1() );
AddTestCase( "inside class outside package function Name cases()", "cases", TESTOBJ.pubcases() );
AddTestCase( "inside class outside package function Name Cases()", "Cases", TESTOBJ.pubCases() );
AddTestCase( "inside class outside package function Name all()", "all", TESTOBJ.puball() );


test();       // leave this alone.  this executes the test cases and
              // displays results.
