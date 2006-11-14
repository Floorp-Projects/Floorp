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
* Date: 01 May 2001
*
* SUMMARY:  Regression test for Bugzilla bug 31316:
* "Rhino: Regexp matches return garbage"
*
* See http://bugzilla.mozilla.org/show_bug.cgi?id=31316
*/
//-------------------------------------------------------------------------------------------------

var SECTION = "eregress_31316";
var VERSION = "";
var TITLE   = "Regression test for Bugzilla bug 31316";
var bug = "31316";

startTest();
writeHeaderToLog(SECTION + " " + TITLE);
var testcases = getTestCases();
test();

function getTestCases() {
	var array = new Array();
	var item = 0;

	var cnEmptyString = '';
	var status = '';
	var pattern = '';
	var string = '';
	var actualmatch = '';
	var expectedmatch = '';


	status = inSection(1);
	pattern = /<([^\/<>][^<>]*[^\/])>|<([^\/<>])>/;
	string = '<p>Some<br />test</p>';
	actualmatch = string.match(pattern);
	expectedmatch = Array('<p>', undefined, 'p');
	array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	return array;
}
