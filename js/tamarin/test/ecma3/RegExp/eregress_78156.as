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
