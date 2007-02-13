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


import PublicFunctionName.*;

class PublicFunctionNameClass {
    // constructor
    function PublicFunctionNameClass() { res = "EmptyName"; }

    // not the constructor but looks like it
    function publicFunctionNameClass() { return "not the constructor" }

    public function a1 () { return "a1"; }
    public function a_1 () { return "a_1"; }
    public function _a1 () { return "_a1"; }
    public function __a1 () { return "__a1"; }
    public function _a1_ () { return "_a1_"; }
    public function __a1__ () { return "__a1__"; }
    public function $a1 () { return "$a1"; }
    public function a$1 () { return "a$1"; }
    public function a1$ () { return "a1$"; }
    public function A1 () { return "A1"; }
    public function cases () { return "cases"; }
    public function Cases () { return "Cases"; }
    public function abcdefghijklmnopqrstuvwxyz0123456789$_ () { return "all"; }
}

function b1 () { return "b1"; }
function b_1 () { return "b_1"; }
function _b1 () { return "_b1"; }
function __b1 () { return "__b1"; }
function _b1_ () { return "_b1_"; }
function __b1__ () { return "__b1__"; }
function $b1 () { return "$b1"; }
function b$1 () { return "b$1"; }
function b1$ () { return "b1$"; }
function B1 () { return "B1"; }
function bcases () { return "bcases"; }
function BCases () { return "BCases"; }
function abbcdefghijklmnopqrstuvwxyz0123456789$_ () { return "ball"; }

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
AddTestCase( "inside class inside package function Name constructor different case", "not the constructor", TESTOBJ.pubTestConst() );

// outside class inside package
AddTestCase( "outside class inside package a1()", "a1", puba1() );
AddTestCase( "outside class inside package a_1()", "a_1", puba_1() );
AddTestCase( "outside class inside package function Name _a1()", "_a1", pub_a1() );
AddTestCase( "outside class inside package function Name __a1()", "__a1", pub__a1() );
AddTestCase( "outside class inside package function Name _a1_()", "_a1_", pub_a1_() );
AddTestCase( "outside class inside package function Name __a1__()", "__a1__", pub__a1__() );
AddTestCase( "outside class inside package function Name $a1()", "$a1", pub$a1() );
AddTestCase( "outside class inside package function Name a$1()", "a$1", puba$1() );
AddTestCase( "outside class inside package function Name a1$()", "a1$", puba1$() );
AddTestCase( "outside class inside package function Name A1()", "C1", pubC1() );
AddTestCase( "outside class inside package function Name cases()", "cases", pubcases() );
AddTestCase( "outside class inside package function Name Cases()", "Cases", pubCases() );
AddTestCase( "outside class inside package function Name all()", "all", puball() );

// inside class outside package
TESTOBJ = new PublicFunctionNameClass();
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
AddTestCase( "inside class outside package function Name constructor different case", "not the constructor", TESTOBJ.publicFunctionNameClass() );

// outside class outside package
AddTestCase( "outside class outside package b1()", "b1", b1() );
AddTestCase( "outside class outside package b_1()", "b_1", b_1() );
AddTestCase( "outside class outside package function Name _b1()", "_b1", _b1() );
AddTestCase( "outside class outside package function Name __b1()", "__b1", __b1() );
AddTestCase( "outside class outside package function Name _b1_()", "_b1_", _b1_() );
AddTestCase( "outside class outside package function Name __b1__()", "__b1__", __b1__() );
AddTestCase( "outside class outside package function Name $b1()", "$b1", $b1() );
AddTestCase( "outside class outside package function Name b$1()", "b$1", b$1() );
AddTestCase( "outside class outside package function Name b1$()", "b1$", b1$() );
AddTestCase( "outside class outside package function Name B1()", "B1", B1() );
AddTestCase( "outside class outside package function Name bcases()", "bcases", bcases() );
AddTestCase( "outside class outside package function Name BCases()", "BCases", BCases() );
AddTestCase( "outside class outside package function Name ball()", "ball", abbcdefghijklmnopqrstuvwxyz0123456789$_() );


test();       // leave this alone.  this executes the test cases and
              // displays results.
