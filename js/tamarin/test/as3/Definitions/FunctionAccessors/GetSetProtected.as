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

import GetSetProtected.*;

var SECTION = "FunctionAccessors";
var VERSION = "AS3"; 
var TITLE   = "Function Accessors";
var BUGNUMBER = "";

startTest();

OBJ = new GetSetProtected();

/* 
 *
 * These will all need to be changed when
 * the protected attribute is implemented.
 *
 *
 */


AddTestCase("Protected getter, boolean", true, OBJ.getBoolean());
AddTestCase("Protected setter, boolean", false, OBJ.setBoolean(false));
AddTestCase("Protected getter, uint", 101, OBJ.getUint());
AddTestCase("Protected setter, uint", 5, OBJ.setUint(5));
AddTestCase("Protected getter, array", "1,2,3", OBJ.getArray().toString());
AddTestCase("Protected setter, array", "one,two,three", OBJ.setArray(["one","two","three"]).toString());
AddTestCase("Protected getter, string", "myString", OBJ.getString());
AddTestCase("Protected setter, string", "new string", OBJ.setString("new string"));
AddTestCase("Protected getter, no type", "no type", OBJ.getNoType());
AddTestCase("Protected setter, no type", 2012, OBJ.setNoType(2012));

test();

