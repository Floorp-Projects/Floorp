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

var SECTION = "Expressions";        // provide a document reference (ie, Actionscript section)
var VERSION = "AS3";        		// Version of ECMAScript or ActionScript
var TITLE   = "as operator";        // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                // leave this alone

AddTestCase( "'string' as String", "string", ("string" as String));
AddTestCase( "'string' as Number", null, ("string" as Number));
AddTestCase( "'string' as int", null, ("string" as int));
AddTestCase( "'string' as uint", null, ("string" as uint));
AddTestCase( "'string' as Boolean", null, ("string" as Boolean));
AddTestCase( "'string' as Object", "string", ("string" as Object));
AddTestCase( "null as String", null, (null as String));
AddTestCase( "null as Number", null, (null as Number)); // bug 141358
AddTestCase( "null as int", null, (null as int));
AddTestCase( "null as uint", null, (null as uint));
AddTestCase( "null as Boolean", null, (null as Boolean));
AddTestCase( "null as Object", null, (null as Object));
AddTestCase( "undefined as String", null, (undefined as String)); // bug 131810
AddTestCase( "undefined as Number", null, (undefined as Number));
AddTestCase( "undefined as int", null, (undefined as int));
AddTestCase( "undefined as uint", null, (undefined as uint));
AddTestCase( "undefined as Boolean", null, (undefined as Boolean));
AddTestCase( "undefined as Object", null, (undefined as Object));
AddTestCase( "true as String", null, (true as String)); // bug 131810
AddTestCase( "true as Number", null, (true as Number));
AddTestCase( "true as int", null, (true as int));
AddTestCase( "true as uint", null, (true as uint));
AddTestCase( "true as Boolean", true, (true as Boolean));
AddTestCase( "true as Object", true, (true as Object));
AddTestCase( "false as String", null, (false as String));
AddTestCase( "false as Number", null, (false as Number));
AddTestCase( "false as int", null, (false as int));
AddTestCase( "false as uint", null, (false as uint));
AddTestCase( "false as Boolean", false, (false as Boolean));
AddTestCase( "false as Object", false, (false as Object));
AddTestCase( "1.23 as String", null, (1.23 as String));
AddTestCase( "1.23 as Number", 1.23, (1.23 as Number));
AddTestCase( "1.23 as int", null, (1.23 as int));
AddTestCase( "1.23 as uint", null, (1.23 as uint));
AddTestCase( "1.23 as Boolean", null, (1.23 as Boolean));
AddTestCase( "1.23 as Object", 1.23, (1.23 as Object));
AddTestCase( "-1.23 as String", null, (-1.23 as String));
AddTestCase( "-1.23 as Number", -1.23, (-1.23 as Number));
AddTestCase( "-1.23 as int", null, (-1.23 as int));
AddTestCase( "-1.23 as uint", null, (-1.23 as uint));
AddTestCase( "-1.23 as Boolean", null, (-1.23 as Boolean));
AddTestCase( "-1.23 as Object", -1.23, (-1.23 as Object));
AddTestCase( "NaN as String", null, (NaN as String));
AddTestCase( "NaN as Number", NaN, (NaN as Number));
AddTestCase( "NaN as int", null, (NaN as int));
AddTestCase( "NaN as uint", null, (NaN as uint));
AddTestCase( "NaN as Boolean", null, (NaN as Boolean));
AddTestCase( "NaN as Object", NaN, (NaN as Object));

test();       // leave this alone.  this executes the test cases and
              // displays results.


