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
* Date:    31 August 2002
* SUMMARY: RegExp conformance test
* See http://bugzilla.mozilla.org/show_bug.cgi?id=165353
*
*/
//-----------------------------------------------------------------------------

var SECTION = "eregress_165353";
var VERSION = "";
var TITLE   = "RegExp conformance test";
var bug = "165353";

startTest();
writeHeaderToLog(SECTION + " " + TITLE);
var testcases = getTestCases();
test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	var status = '';
	var pattern = '';
	var string = '';
	var actualmatch = '';
	var expectedmatch = '';
	var expectedmatches = new Array();

	pattern = /^([a-z]+)*[a-z]$/;
	  status = inSection(1);
	  string = 'a';
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('a', undefined);
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  status = inSection(2);
	  string = 'ab';
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('ab', 'a');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  status = inSection(3);
	  string = 'abc';
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('abc', 'ab');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());


	string = 'www.netscape.com';
	  status = inSection(4);
	  pattern = /^(([a-z]+)*[a-z]\.)+[a-z]{2,}$/;
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('www.netscape.com', 'netscape.', 'netscap');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	  // add one more capturing parens to the previous regexp -
	  status = inSection(5);
	  pattern = /^(([a-z]+)*([a-z])\.)+[a-z]{2,}$/;
	  actualmatch = string.match(pattern);
	  expectedmatch = Array('www.netscape.com', 'netscape.', 'netscap', 'e');
	  array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	return array;
}
