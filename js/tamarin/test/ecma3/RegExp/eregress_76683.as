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
* SUMMARY: Regression test for Bugzilla bug 76683 on Rhino:
* "RegExp regression (NullPointerException)"
*
* See http://bugzilla.mozilla.org/show_bug.cgi?id=76683
*/
//-------------------------------------------------------------------------------------------------
var SECTION = "eregress_76683";
var VERSION = "";
var TITLE   = "Regression test for Bugzilla bug 76683";
var bug = "76683";

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


/*
 * Rhino (2001-04-19) crashed on the 3rd regular expression below.
 * It didn't matter what the string was. No problem in SpiderMonkey -
 */
string = 'abc';
    status = inSection(1);
    pattern = /(<!--([^-]|-[^-]|--[^>])*-->)|(<([\$\w:\.\-]+)((([ ][^\/>]*)?\/>)|(([ ][^>]*)?>)))/;
    actualmatch = string.match(pattern);
    expectedmatch = null;
    array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

    status = inSection(2);
    pattern = /(<!--([^-]|-[^-]|--[^>])*-->)|(<(tagPattern)((([ ][^\/>]*)?\/>)|(([ ][^>]*)?>)))/;
    actualmatch = string.match(pattern);
    expectedmatch = null;
    array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

    // This was the one causing a Rhino crash -
    status = inSection(3);
    pattern = /(<!--([^-]|-[^-]|--[^>])*-->)|(<(tagPattern)((([ ][^\/>]*)?\/>)|(([ ][^>]*)?>)))|(<\/tagPattern[^>]*>)/;
    actualmatch = string.match(pattern);
    expectedmatch = null;
    array[item++] = new TestCase(SECTION, status, expectedmatch, actualmatch);

	return array;
}
