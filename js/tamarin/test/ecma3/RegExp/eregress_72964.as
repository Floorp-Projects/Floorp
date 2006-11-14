/*
* The contents of this file are subject to the Netscape Public
* License Version 1.1 (the "License"); you may not use this file
* except in compliance with the License. You may obtain a copy of
* the License at http://www.mozilla.org/NPL/
*
* Software distributed under the License is distributed on an "AS  IS"
* basis, WITHOUT WARRANTY OF ANY KIND, either expressed
* or implied. See the License for the specific language governing
* rights and limitations under the License.
*
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation.
* All Rights Reserved.
*
* Contributor(s): pschwartau@netscape.com
* Date: 2001-07-17
*
* SUMMARY: Regression test for Bugzilla bug 72964:
* "String method for pattern matching failed for Chinese Simplified (GB2312)"
*
* See http://bugzilla.mozilla.org/show_bug.cgi?id=72964
*/
//-----------------------------------------------------------------------------

var SECTION = "eregress_72964";
var VERSION = "";
var TITLE   = "Testing regular expressions containing non-Latin1 characters";
var bug = "72964";

startTest();
writeHeaderToLog(SECTION + " " + TITLE);
var testcases = getTestCases();
test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	var cnSingleSpace = ' ';
	var status = '';
	var pattern = '';
	var string = '';
	var actualmatch = '';
	var expectedmatch = '';


	pattern = /[\S]+/;
	    // 4 low Unicode chars = Latin1; whole string should match
	    status = inSection(1);
	    string = '\u00BF\u00CD\u00BB\u00A7';
	    actualmatch = string.match(pattern);
	    expectedmatch = Array(string);
	    array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	    // Now put a space in the middle; first half of string should match
	    status = inSection(2);
	    string = '\u00BF\u00CD \u00BB\u00A7';
	    actualmatch = string.match(pattern);
	    expectedmatch = Array('\u00BF\u00CD');
	    array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());


	    // 4 high Unicode chars = non-Latin1; whole string should match
	    status = inSection(3);
	    string = '\u4e00\uac00\u4e03\u4e00';
	    actualmatch = string.match(pattern);
	    expectedmatch = Array(string);
	    array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	    // Now put a space in the middle; first half of string should match
	    status = inSection(4);
	    string = '\u4e00\uac00 \u4e03\u4e00';
	    actualmatch = string.match(pattern);
	    expectedmatch = Array('\u4e00\uac00');
	    array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	return array;
}
