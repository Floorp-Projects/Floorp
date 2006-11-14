/*
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
* The Original Code is mozilla.org code.
*
* The Initial Developer of the Original Code is Netscape
* Communications Corporation.  Portions created by Netscape are
* Copyright (C) 1998 Netscape Communications Corporation.
* All Rights Reserved.
*
* Contributor(s): k.mike@gmx.net, pschwartau@netscape.com
* Date: 12 October 2001
*
* SUMMARY: Regression test for string.replace bug 104375
* See http://bugzilla.mozilla.org/show_bug.cgi?id=104375
*/
//-----------------------------------------------------------------------------
    var SECTION = "eregress_104375";
    var VERSION = "";
    var bug = 104375;

    startTest();
    var TITLE   = "Testing string.replace() with backreferences";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;


    var UBound = 0;
    var status = '';
    var statusitems = [];
    var actual = '';
    var actualvalues = [];
    var expect= '';
    var expectedvalues = [];


    /*
     * Use the regexp to replace 'uid=31' with 'uid=15'
     *
     * In the second parameter of string.replace() method,
     * "$1" refers to the first backreference: 'uid='
     */
    var str = 'uid=31';
    var re = /(uid=)(\d+)/;
    
    // try the numeric literal 15
    status = inSection(1);
    actual  = str.replace (re, "$1" + 15);
    expect = 'uid=15';
    array[item++] = new TestCase(SECTION, status, expect, actual);
    
    // try the string literal '15'
    status = inSection(2);
    actual  = str.replace (re, "$1" + '15');
    expect = 'uid=15';
    array[item++] = new TestCase(SECTION, status, expect, actual);
    
    // try a letter before the '15'
    status = inSection(3);
    actual  = str.replace (re, "$1" + 'A15');
    expect = 'uid=A15';
    array[item++] = new TestCase(SECTION, status, expect, actual);

    return array;
}
