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
* Date: 06 February 2001
*
* SUMMARY:  Arose from Bugzilla bug 78156:
* "m flag of regular expression does not work with $"
*
* See http://bugzilla.mozilla.org/show_bug.cgi?id=78156
*
* The m flag means a regular expression should search strings
* across multiple lines, i.e. across '\n', '\r'.
*/
//-------------------------------------------------------------------------------------------------
var SECTION = "eregress_78156";
var VERSION = "";
var TITLE   = "Testing regular expressions with  ^, $, and the m flag -";
var bug = "78156";

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
 * All patterns have an m flag; all strings are multiline.
 * Looking for digit characters at beginning/end of lines.
 */

string = 'aaa\n789\r\nccc\r\n345';
    status = inSection(1);
    pattern = /^\d/gm;
    actualmatch = string.match(pattern);
    expectedmatch = ['7', '3'];
    array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

    status = inSection(2);
    pattern = /\d$/gm;
    actualmatch = string.match(pattern);
    expectedmatch = ['9','5'];
    array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

string = 'aaa\n789\r\nccc\r\nddd';
    status = inSection(3);
    pattern = /^\d/gm;
    actualmatch = string.match(pattern);
    expectedmatch = ['7'];
    array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

    status = inSection(4);
    pattern = /\d$/gm;
    actualmatch = string.match(pattern);
    expectedmatch = ['9'];
    array[item++] = new TestCase(SECTION, status, expectedmatch.toString(), actualmatch.toString());

	return array;
}
