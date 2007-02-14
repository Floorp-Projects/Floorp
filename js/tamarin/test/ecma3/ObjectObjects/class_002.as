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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   pschwartau@netscape.com
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
/*
 * Date: 14 Mar 2001
 *
 * SUMMARY: Testing the [[Class]] property of native constructors.
 * See ECMA-262 Edition 3 13-Oct-1999, Section 8.6.2 re [[Class]] property.
 *
 * Same as class-001.js - but testing the constructors here, not object instances.
 * Therefore we expect the [[Class]] property to equal 'Function' in each case.
 *
 * The getJSClass() function we use is in a utility file, e.g. "shell.js"
 */
//-------------------------------------------------------------------------------------------------
var SECTION = "class_002";
var VERSION = "";
var TITLE   = "Testing the internal [[Class]] property of native constructors";
var bug = "(none)";

startTest();
writeHeaderToLog(SECTION + " " + TITLE);
var testcases = getTestCases();
test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	var status = '';
	var actual = '';
	var expect= '';

	/*
	 * We set the expect variable each time only for readability.
	 * We expect 'Function' every time; see discussion above -
	 */
	status = 'Object';
	actual = getJSClass(Object);
	expect = 'Object';
	array[item++] = new TestCase(SECTION, status, expect, actual);

	status = 'Function';
	actual = getJSClass(Function);
	expect = 'Function';
	array[item++] = new TestCase(SECTION, status, expect, actual);

	status = 'Array';
	actual = getJSClass(Array);
	expect = 'Array';
	array[item++] = new TestCase(SECTION, status, expect, actual);

	status = 'String';
	actual = getJSClass(String);
	expect = 'String';
	array[item++] = new TestCase(SECTION, status, expect, actual);

	status = 'Boolean';
	actual = getJSClass(Boolean);
	expect = 'Boolean';
	array[item++] = new TestCase(SECTION, status, expect, actual);

	status = 'Number';
	actual = getJSClass(Number);
	expect = 'Number';
	array[item++] = new TestCase(SECTION, status, expect, actual);

	status = 'Date';
	actual = getJSClass(Date);
	expect = 'Date';
	array[item++] = new TestCase(SECTION, status, expect, actual);

	status = 'RegExp';
	actual = getJSClass(RegExp);
	expect = 'RegExp';
	array[item++] = new TestCase(SECTION, status, expect, actual);

	status = 'Error';
	actual = getJSClass(Error);
	expect = 'Error';
	array[item++] = new TestCase(SECTION, status, expect, actual);

	return array;
}
