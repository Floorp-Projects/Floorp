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
 * The Original Code is [Open Source Virtual Machine].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
*
*
* Date:    09 September 2002
* SUMMARY: Test for TypeError on invalid default string value of object
* See ECMA reference at http://bugzilla.mozilla.org/show_bug.cgi?id=167325
*
*/
//-----------------------------------------------------------------------------
var SECTION = "e8_6_2_6_001";
var VERSION = "";
var TITLE   = "Test for TypeError on invalid default string value of object";
var bug = "167325";

startTest();
writeHeaderToLog(SECTION + " " + TITLE);
var testcases = getTestCases();
test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	var TEST_PASSED = 'TypeError';
	var TEST_FAILED = 'Generated an error, but NOT a TypeError!';
	var TEST_FAILED_BADLY = 'Did not generate ANY error!!!';
	var status = '';
	var actual = '';
	var expect= '';


	status = inSection(1);
	expect = TEST_PASSED;
	actual = TEST_FAILED_BADLY;
	/*
	 * This should generate a TypeError. See ECMA reference
	 * at http://bugzilla.mozilla.org/show_bug.cgi?id=167325
	 */
	try
	{
	  var obj = {toString: function() {return new Object();}}
	  obj == 'abc';
	}
	catch(e)
	{
	  if (e instanceof TypeError)
	    actual = TEST_PASSED;
	  else
	    actual = TEST_FAILED;
	}
	array[item++] = new TestCase(SECTION, status, expect, actual);

	return array;
}
