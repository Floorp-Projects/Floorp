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


    var SECTION = "10.1.4.1";
    var VERSION = "";
    var TITLE   = "Entering An Execution Context";
	var bug     = "none";

    startTest();
    writeHeaderToLog( SECTION + " "+ TITLE);
    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    var y;
    var x = 1;
	z = 11;  // dynamic var z

    if (delete y)
        array[item++] = new TestCase(SECTION, "Expected *NOT* to be able to delete y", "fail", "fail");

	if (!delete z)
        array[item++] = new TestCase(SECTION, "Expected to be able to delete z", "fail", "fail");

    if (typeof x == "undefined")
        array[item++] = new TestCase(SECTION, "x did not remain defined after eval()", "fail", "fail");
    else if (x != 1)
        array[item++] = new TestCase(SECTION, "x did not retain it's value after eval()", "fail", "fail");


    if (delete x)
        array[item++] = new TestCase(SECTION, "Expected to be able to delete x", "fail", "fail");

	array[item++] = new TestCase(SECTION, "All tests passed", true, true);

    return ( array );
}
