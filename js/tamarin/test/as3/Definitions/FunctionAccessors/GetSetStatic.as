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

import GetSetStaticPackage.*;
import GetSetStaticSameName.*;

var SECTION = "FunctionAccessors";
var VERSION = "AS3";
var TITLE   = "Function Accessors";
var BUGNUMBER = "";

startTest();

AddTestCase("Static getter var:int", -10, GetSetStatic.y);
AddTestCase("Static setter var:int", 23334, (GetSetStatic.y = 23334, GetSetStatic.y));
AddTestCase("Static getter var:Array", "1,2,3", GetSetStatic.x.toString());
AddTestCase("Static setter var:Array", "4,5,6", (GetSetStatic.x = new Array(4,5,6), GetSetStatic.x.toString()));
AddTestCase("Static getter var:Boolean", true, GetSetStatic.boolean);
AddTestCase("Static setter var:Boolean", false, (GetSetStatic.boolean = false, GetSetStatic.boolean));
AddTestCase("Static getter var:uint", 1, GetSetStatic.u);
AddTestCase("Static setter var:uint", 42, (GetSetStatic.u = 42, GetSetStatic.u));
AddTestCase("Static getter var:String", "myString", GetSetStatic.string);
AddTestCase("Static setter var:String", "new string", (GetSetStatic.string = "new string", GetSetStatic.string));

// Attempt to access the private vars directly
// New instances of vars will be created since the private vars are not accessible
try{
	var res = "not run";
	AddTestCase("Access private var:Array", 4, GetSetStatic._x = 4);
	res = "no exception";
} catch (e1) {
	res = "exception";
} finally {
	AddTestCase("Access private var:Array", "no exception", res);
}

try{
	var res = "not run";
	AddTestCase("Access private var:int", 4, GetSetStatic._y = 4);
	res = "no exception";
} catch (e2) {
	res = "exception";
} finally {
	AddTestCase("Access private var:int", "no exception", res);
}

try{
	var res = "not run";
	AddTestCase("Access private var:Boolean", 4, GetSetStatic._b = 4);
	res = "no exception";
} catch (e3) {
	res = "exception";
} finally {
	AddTestCase("Access private var:Boolean", "no exception", res);
}

try{
	var res = "not run";
	AddTestCase("Access private var:uint", 4, GetSetStatic._u = 4);
	res = "no exception";
} catch (e4) {
	res = "exception";
} finally {
	AddTestCase("Access private var:uint", "no exception", res);
}

try{
	var res = "not run";
	AddTestCase("Access private var:String", 4, GetSetStatic._s = 4);
	res = "no exception";
} catch (e5) {
	res = "exception";
} finally {
	AddTestCase("Access private var:String", "no exception", res);
}

// call setter from setter
GetSetStatic.sfs2 = 55;
AddTestCase("Call setter from setter", 55, GetSetStatic.sfs1);
AddTestCase("Call setter from setter", 55, GetSetStatic.sfs2);

// call setter from getter
AddTestCase("Call setter from getter", 0, GetSetStatic.sfg2);
AddTestCase("Call setter from getter", "PASSED", GetSetStatic.sfg1);

// call getter from setter
GetSetStatic.gfs1 = "FAILED";// setter for gfs1 should make the string 'PASSED'
AddTestCase("Call getter from setter", "PASSED", GetSetStatic.gfs1);
AddTestCase("Call getter from setter", "PASSED", GetSetStatic.gfs2);

// call getter from getter

AddTestCase("Call getter from getter", "PASSED", GetSetStatic.gfg1);
AddTestCase("Call getter from getter", "PASSED", GetSetStatic.gfg2);

// Try to access a getter in a class that's the same name as the package it's in
// See bug 133422; test case needs to be updated when/if bug is fixed
try{
	var res = "not run";
	trace(GetSetStaticSameName.y);
	res = "no exception";
} catch (e6) {
	res = "exception";
} finally {
	AddTestCase("Access getter in class in package of same name", "exception", res);
}

// Attempt to access non-static var
// See bug 117661; test case needs to be updated when/if bug is fixed
try{
	var res = "not run";
	var someVar = GetSetStatic.n;
	res = "no exception";
} catch (e7) {
	res = "exception";
} finally {
	AddTestCase("Get non-static var:Number", "exception", res);
}

// Set non-static var
// See bug 133468; test case needs to be updated when/if bug is fixed
try{
	var res = "not run";
	GetSetStatic.n = 5.55;
	res = GetSetStatic.n;
} catch (e8) {
	res = "exception";
} finally {
	AddTestCase("Set non-static var:Number", 5.55, res);
}

test();

