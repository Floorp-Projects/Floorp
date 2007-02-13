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

import GetSetDynamicAccess.*;

var SECTION = "FunctionAccessors";
var VERSION = "AS3"; 
var TITLE   = "Function Accessors";
var BUGNUMBER = "";

startTest();

var OBJ = new GetSetPrivate();

AddTestCase("Get private var:int", -10, OBJ["y"]);
AddTestCase("Set private var:int", 23334, (OBJ["y"] = 23334, OBJ["y"]));
AddTestCase("Get private var:Array", "1,2,3", OBJ["x"].toString());
AddTestCase("Set private var:Array", "4,5,6", (OBJ["x"] = new Array(4,5,6), OBJ["x"].toString()));
AddTestCase("Get private var:Boolean", true, OBJ["boolean"]);
AddTestCase("Set private var:Boolean", false, (OBJ["boolean"] = false, OBJ["boolean"]));
AddTestCase("Get private var:uint", 1, OBJ["u"]);
AddTestCase("Set private var:uint", 42, (OBJ["u"] = 42, OBJ["u"]));
AddTestCase("Get private var:String", "myString", OBJ["string"]);
AddTestCase("Set private var:String", "new string", (OBJ["string"] = "new string", OBJ["string"]));

// Attempt to access the private vars directly
try{
	var res = "not run";
	OBJ["_x"] = 4;
	res = "no exception";
} catch (e1) {
	res = "exception";
} finally {
	AddTestCase("Access private var:Array", "exception", res);
}

try{
	var res = "not run";
	OBJ["_y"] = 4;
	res = "no exception";
} catch (e2) {
	res = "exception";
} finally {
	AddTestCase("Access private var:int", "exception", res);
}

try{
	var res = "not run";
	OBJ["_b"] = 4;
	res = "no exception";
} catch (e3) {
	res = "exception";
} finally {
	AddTestCase("Access private var:Boolean", "exception", res);
}

try{
	var res = "not run";
	OBJ["_u"] = 4;
	res = "no exception";
} catch (e4) {
	res = "exception";
} finally {
	AddTestCase("Access private var:uint", "exception", res);
}

try{
	var res = "not run";
	OBJ["_s"] = 4;
	res = "no exception";
} catch (e5) {
	res = "exception";
} finally {
	AddTestCase("Access private var:String", "exception", res);
}

// call setter from setter
OBJ["sfs2"] = 55;
AddTestCase("Call setter from setter", 55, OBJ["sfs1"]);
AddTestCase("Call setter from setter", 55, OBJ["sfs2"]);

// call setter from getter
AddTestCase("Call setter from getter", 0, OBJ["sfg2"]);
AddTestCase("Call setter from getter", "PASSED", OBJ["sfg1"]);

// call getter from setter
OBJ["gfs1"] = "FAILED";// setter for gfs1 should make the string 'PASSED'
AddTestCase("Call getter from setter", "PASSED", OBJ["gfs1"]);
AddTestCase("Call getter from setter", "PASSED", OBJ["gfs2"]);

// call getter from getter

AddTestCase("Call getter from getter", "PASSED", OBJ["gfg1"]);
AddTestCase("Call getter from getter", "PASSED", OBJ["gfg2"]);

//define a getter for a property and call the undefined setter
try{
	var res = "not run";
	OBJ["noSetter"] = "test";
	res = "no exception";
} catch (e6) {
	res = "exception";
} finally {
	// This needs to be changed when bug 143401 is fixed
	AddTestCase("call undefined setter", "exception", res);
}

//define a setter for a property and call the undefined getter
try{
	var res = "not run";
	OBJ["noGetter"];
	res = "no exception";
} catch (e7) {
	res = "exception";
} finally {
	AddTestCase("call undefined getter", "exception", res);
}


test();

