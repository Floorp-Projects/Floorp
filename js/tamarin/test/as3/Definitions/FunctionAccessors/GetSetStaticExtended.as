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

import GetSetStaticExtended.*;

var SECTION = "FunctionAccessors";
var VERSION = "AS3";
var TITLE   = "Function Accessors";
var BUGNUMBER = "";

startTest();

var res;

/*
 *
 * statics are not inherited
 * New instances of vars will be created since the statics don't exist
 *
 */

try {
	AddTestCase("Static getter var:int", undefined, GetSetStatic.y);
	res  = "no exception";
} catch(e1) {
	res = "exception";
}
AddTestCase("Static getter var:int", "no exception", res);

try {
	AddTestCase("Static setter var:int", 23334, (GetSetStatic.y = 23334, GetSetStatic.y));
	res  = "no exception";
	} catch(e2) {
		res = "exception";
	}
AddTestCase("Static setter var:int", "no exception", res);

try {
	// exception calling toString() on undefined GetSetStatic.x
	GetSetStatic.x.toString();
	res  = "no exception";
	} catch(e3) {
		res = "exception";
	}
AddTestCase("Static getter var:Array", "exception", res);

try {
	AddTestCase("Static setter var:Array", "4,5,6", (GetSetStatic.x = new Array(4,5,6), GetSetStatic.x.toString()));
	res  = "no exception";
} catch(e4) {
	res = "exception";
}
AddTestCase("Static setter var:Array", "no exception", res);

try {
	AddTestCase("Static getter var:Boolean", undefined, GetSetStatic.boolean);
	res  = "no exception";
} catch(e5) {
	res = "exception";
}
AddTestCase("Static getter var:Boolean", "no exception", res);

try {
	AddTestCase("Static setter var:Boolean", false, (GetSetStatic.boolean = false, GetSetStatic.boolean));
	res  = "no exception";
	} catch(e6) {
		res = "exception";
	}
AddTestCase("Static setter var:Boolean", "no exception", res);

try {
	AddTestCase("Static getter var:uint", undefined, GetSetStatic.u);
	res  = "no exception";
	} catch(e7) {
		res = "exception";
	}
AddTestCase("Static getter var:uint", "no exception", res);
try {
	AddTestCase("Static setter var:uint", 42, (GetSetStatic.u = 42, GetSetStatic.u));
	res  = "no exception";
	} catch(e8) {
		res = "exception";
	}
AddTestCase("Static setter var:uint", "no exception", res);

try {
	AddTestCase("Static getter var:String", undefined, GetSetStatic.string);
	res  = "no exception";
	} catch(e9) {
		res = "exception";
	}
AddTestCase("Static getter var:String", "no exception", res);

try {
	AddTestCase("Static setter var:String", "new string", (GetSetStatic.string = "new string", GetSetStatic.string));
	res  = "no exception";
	} catch(e10) {
		res = "exception";
	}
AddTestCase("Static setter var:String", "no exception", res);

test();

