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
 
 import GetSetAllowedNames.*;
 
 
 var SECTION = "FunctionAccessors";
 var VERSION = "AS3"; 
 var TITLE   = "Function Accessors";
 var BUGNUMBER = "";
 

startTest();


var TESTOBJ = new GetSetAllowedNames();

AddTestCase( "getter name _a1", "_a1", TESTOBJ._a1);
AddTestCase( "getter name _a1_", "_a1_", TESTOBJ._a1_);
AddTestCase( "getter name __a1__", "__a1__", TESTOBJ.__a1__);
AddTestCase( "getter name $a1", "$a1", TESTOBJ.$a1);
AddTestCase( "getter name a$1", "a$1", TESTOBJ.a$1);
AddTestCase( "getter name a1$", "a1$", TESTOBJ.a1$);
AddTestCase( "getter name A1", "A1", TESTOBJ.A1);
AddTestCase( "getter name cases", "cases", TESTOBJ.cases);
AddTestCase( "getter name Cases", "Cases", TESTOBJ.Cases);
AddTestCase( "getter name abcdefghijklmnopqrstuvwxyz0123456789$_", "all", TESTOBJ.abcdefghijklmnopqrstuvwxyz0123456789$_);
AddTestCase( "getter name get", "get", TESTOBJ.get);


AddTestCase( "setter name _a1", "new _a1", (TESTOBJ._a1 = "new _a1", TESTOBJ._a1));
AddTestCase( "setter name _a1_", "new _a1_", (TESTOBJ._a1_ = "new _a1_", TESTOBJ._a1_));
AddTestCase( "setter name __a1__", "new __a1__", (TESTOBJ.__a1__ = "new __a1__", TESTOBJ.__a1__));
AddTestCase( "setter name $a1", "new $a1", (TESTOBJ.$a1 = "new $a1", TESTOBJ.$a1));
AddTestCase( "setter name a$1", "new a$1", (TESTOBJ.a$1 = "new a$1", TESTOBJ.a$1));
AddTestCase( "setter name a1$", "new a1$", (TESTOBJ.a1$ = "new a1$", TESTOBJ.a1$));
AddTestCase( "setter name A1", "new A1", (TESTOBJ.A1 = "new A1", TESTOBJ.A1));
AddTestCase( "setter name cases", "new cases", (TESTOBJ.cases = "new cases", TESTOBJ.cases));
AddTestCase( "setter name Cases", "new Cases", (TESTOBJ.Cases = "new Cases", TESTOBJ.Cases));
AddTestCase( "setter name abcdefghijklmnopqrstuvwxyz0123456789$_", "new all", (TESTOBJ.abcdefghijklmnopqrstuvwxyz0123456789$_ = "new all", TESTOBJ.abcdefghijklmnopqrstuvwxyz0123456789$_));
AddTestCase( "setter name set", "new set", (TESTOBJ.set = "new set", TESTOBJ.set));

var f1 = function () {}
var f2 = function () { var i = 5; }
AddTestCase( "getter function keyword, different capitalization", "function Function() {}", TESTOBJ.FuncTion.toString());
AddTestCase( "setter function keyword, different capitalization", f2, (TESTOBJ.FuncTion = f2, TESTOBJ.FuncTion));

AddTestCase( "getter name same as constructor, different capitalization", "constructor, different case", TESTOBJ.getSetAllowedNames);
AddTestCase( "setter name same as constructor, different capitalization", "constructor, different case new", (TESTOBJ.getSetAllowedNames = "constructor, different case new", TESTOBJ.getSetAllowedNames));

AddTestCase( "getter class keyword, different capitalization", "class", TESTOBJ.Class);
AddTestCase( "setter class keyword, different capitalization", "class new", (TESTOBJ.Class = "class new", TESTOBJ.Class));

test();
