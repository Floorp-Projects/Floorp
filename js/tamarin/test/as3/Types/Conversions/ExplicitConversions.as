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

var SECTION = "Types: Conversions";
var VERSION = "as3";
var TITLE   = "type conversions";

startTest();

// "string"
AddTestCase("String('string')", "string", String("string"));
AddTestCase("String('')", "", String(""));
AddTestCase("Number('string')", NaN, Number("string"));
AddTestCase("Number('')", 0, Number(""));
AddTestCase("int('string')", 0, int("string"));
AddTestCase("int('')", 0, int(""));
AddTestCase("uint('string')", 0, uint("string"));
AddTestCase("uint('')", 0, uint(""));
AddTestCase("Boolean('string')", true, Boolean("string"));
AddTestCase("Boolean('')", false, Boolean(""));
AddTestCase("Object('string')", "string", Object("string"));
AddTestCase("Object('')", "", Object(""));

// null

AddTestCase( "String(null)", "null", String(null));
AddTestCase( "Number(null)", +0, Number(null));
AddTestCase( "int(null)", +0, int(null));
AddTestCase( "uint(null)", +0, uint(null));
AddTestCase( "Boolean(null)", false, Boolean(null));
AddTestCase( "Object(null)", "[object Object]", Object(null)+"");

// undefined
AddTestCase( "String(undefined)", "undefined", String(undefined));
AddTestCase( "Number(undefined)", NaN, Number(undefined));
AddTestCase( "int(undefined)", +0, int(undefined));
AddTestCase( "uint(undefined)", +0, uint(undefined));
AddTestCase( "Boolean(undefined)", false, Boolean(undefined));
AddTestCase( "Object(undefined)", "[object Object]", Object(undefined)+"");

// true
AddTestCase( "String(true)", "true", String(true));
AddTestCase( "Number(true)", 1, Number(true));
AddTestCase( "int(true)", 1, int(true));
AddTestCase( "uint(true)", 1, uint(true));
AddTestCase( "Boolean(true)", true, Boolean(true));
AddTestCase( "Object(true)", true, Object(true));

// false
AddTestCase( "String(false)", "false", String(false));
AddTestCase( "Number(false)", +0, Number(false));
AddTestCase( "int(false)", +0, int(false));
AddTestCase( "uint(false)", +0, uint(false));
AddTestCase( "Boolean(false)", false, Boolean(false));
AddTestCase( "Object(false)", false, Object(false));

// 1.23
AddTestCase( "String(1.23)", "1.23", String(1.23));
AddTestCase( "Number(1.23)", 1.23, Number(1.23));
AddTestCase( "int(1.23)", 1, int(1.23));
AddTestCase( "uint(1.23)", 1, uint(1.23));
AddTestCase( "Boolean(1.23)", true, Boolean(1.23));
AddTestCase( "Object(1.23)", 1.23, Object(1.23));

// -1.23
AddTestCase( "String(-1.23)", "-1.23", String(-1.23));
AddTestCase( "Number(-1.23)", -1.23, Number(-1.23));
AddTestCase( "int(-1.23)", -1, int(-1.23));
AddTestCase( "uint(-1.23)", 4294967295, uint(-1.23));
AddTestCase( "Boolean(-1.23)", true, Boolean(-1.23));
AddTestCase( "Object(-1.23)", -1.23, Object(-1.23));

// NaN
AddTestCase( "String(NaN)", "NaN", String(NaN));
AddTestCase( "Number(NaN)", NaN, Number(NaN));
AddTestCase( "int(NaN)", +0, int(NaN));
AddTestCase( "uint(NaN)", +0, uint(NaN));
AddTestCase( "Boolean(NaN)", false, Boolean(NaN));
AddTestCase( "Object(NaN)", NaN, Object(NaN));





test();
