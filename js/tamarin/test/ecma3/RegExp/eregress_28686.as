/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Rob Ginda rginda@netscape.com
 */

var SECTION = "eregress_28686";
var VERSION = "";
var TITLE   = "Regression test for Bugzilla bug 28686";
var bug = "28686";

startTest();
writeHeaderToLog(SECTION + " " + TITLE);
var testcases = getTestCases();
test();

function getTestCases() {
	var array = new Array();
	var item = 0;

    var str = 'foo "bar" baz';
    reportCompare ('foo \\"bar\\" baz', str.replace(/([\'\"])/g, "\\$1"),
                   "str.replace failed.");
    array[item++] = new TestCase(SECTION,
    	"str.replace failed.",
    	'foo \\"bar\\" baz',
    	str.replace(/([\'\"])/g, "\\$1"));

	return array;
}
