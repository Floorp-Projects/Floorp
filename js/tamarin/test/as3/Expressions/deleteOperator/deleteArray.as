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
var TITLE   = "delete operator";    // Provide ECMA section title or a description
var BUGNUMBER = "";

startTest();                        // leave this alone

var arr:Array = new Array();
arr[0] = "abc"; // arr.length == 1
arr[1] = "def"; // arr.length == 2
arr[2] = "ghi"; // arr.length == 3

// arr[2] is deleted, but Array.length is not changed
AddTestCase("arr[2]", "ghi", arr[2]);
AddTestCase("delete arr[2] successful", true, delete arr[2]);
AddTestCase("arr[2]", undefined, arr[2]);
AddTestCase("array length after delete arr[2]", 3, arr.length);
AddTestCase("array contents after delete arr[2]", "abc,def,", arr.toString());

AddTestCase("delete arr[2] again", true, delete arr[2]);
AddTestCase("arr[2]", undefined, arr[2]);

AddTestCase("delete non-existent arr[3]", true, delete arr[3]);
AddTestCase("arr[3]", undefined, arr[3]);


test();       // leave this alone.  this executes the test cases and
              // displays results.
