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
* Date:    09 July 2002
* SUMMARY: RegExp conformance test
*
*   These testcases are derived from the examples in the ECMA-262 Ed.3 spec
*   scattered through section 15.10.2.
*
*/
//-----------------------------------------------------------------------------

var SECTION = "e15_10_2_1";
var VERSION = "";
var TITLE   = "RegExp conformance test";
var bug = "(none)";

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

	status = inSection(1);
	pattern = /a|ab/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(2);
	pattern = /((a)|(ab))((c)|(bc))/;
	string = 'abc';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc', 'a', 'a', undefined, 'bc', undefined, 'bc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(3);
	pattern = /a[a-z]{2,4}/;
	string = 'abcdefghi';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abcde');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(4);
	pattern = /a[a-z]{2,4}?/;
	string = 'abcdefghi';
	actualmatch = string.match(pattern);
	expectedmatch = Array('abc');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(5);
	pattern = /(aa|aabaac|ba|b|c)*/;
	string = 'aabaac';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaba', 'ba');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(6);
	pattern = /^(a+)\1*,\1+$/;
	string = 'aaaaaaaaaa,aaaaaaaaaaaaaaa';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aaaaaaaaaa,aaaaaaaaaaaaaaa', 'aaaaa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(7);
	pattern = /(z)((a+)?(b+)?(c))*/;
	string = 'zaacbbbcac';
	actualmatch = string.match(pattern);
	expectedmatch = Array('zaacbbbcac', 'z', 'ac', 'a', undefined, 'c');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(8);
	pattern = /(a*)*/;
	string = 'b';
	actualmatch = string.match(pattern);
	expectedmatch = Array('', "");
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(9);
	pattern = /(a*)b\1+/;
	string = 'baaaac';
	actualmatch = string.match(pattern);
	expectedmatch = Array('b', '');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(10);
	pattern = /(?=(a+))/;
	string = 'baaabac';
	actualmatch = string.match(pattern);
	expectedmatch = Array('', 'aaa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(11);
	pattern = /(?=(a+))a*b\1/;
	string = 'baaabac';
	actualmatch = string.match(pattern);
	expectedmatch = Array('aba', 'a');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(12);
	pattern = /(.*?)a(?!(a+)b\2c)\2(.*)/;
	string = 'baaabaac';
	actualmatch = string.match(pattern);

	expectedmatch = Array('baaabaac', 'ba', undefined, 'abaac');

	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	status = inSection(13);
	pattern = /(?=(a+))/;
	string = 'baaabac';
	actualmatch = string.match(pattern);
	expectedmatch = Array('', 'aaa');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	return array;
}
