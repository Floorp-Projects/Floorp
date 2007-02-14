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
 *   pschwartau@netscape.com, jim@jibbering.com
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
 * Creation Date:   30 May 2001
 * Correction Date: 14 Aug 2001
 *
 * SUMMARY:  Regression test for bugs 83293, 103351
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=83293
 *     http://bugzilla.mozilla.org/show_bug.cgi?id=103351
 *     http://bugzilla.mozilla.org/show_bug.cgi?id=92942
 *
 *
 * ********************   CORRECTION !!!  *****************************
 *
 * When I originally wrote this test, I thought this was true:
 * str.replace(strA, strB) == str.replace(new RegExp(strA),strB).
 * See ECMA-262 Final Draft, 15.5.4.11 String.prototype.replace
 *
 * However, in http://bugzilla.mozilla.org/show_bug.cgi?id=83293
 * Jim Ley points out the ECMA-262 Final Edition changed on this.
 * String.prototype.replace (searchValue, replaceValue), if provided
 * a searchValue that is not a RegExp, is NO LONGER to replace it with
 *
 *                  new RegExp(searchValue)
 * but rather:
 *                  String(searchValue)
 *
 * This puts the replace() method at variance with search() and match(),
 * which continue to follow the RegExp conversion of the Final Draft.
 * It also makes most of this testcase, as originally written, invalid.
 **********************************************************************
 */
//-----------------------------------------------------------------------------
    var SECTION = "eregress_104375";
    var VERSION = "";
    var bug = 103351; // <--- (Outgrowth of original bug 83293)

    startTest();
    var summ_OLD = 'Testing str.replace(strA, strB) == str.replace(new RegExp(strA),strB)';
    var TITLE   = "Testing String.prototype.replace(x,y) when x is a string";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;
    var status = '';
    var actual = '';
    var expect= '';
    var cnEmptyString = '';
    var str = 'abc';
    var strA = cnEmptyString;
    var strB = 'Z';

//////////////////////////  OK, LET'S START OVER //////////////////////////////

    status = 'Section 1 of test';
    actual = 'abc'.replace('a', 'Z');
    expect = 'Zbc';
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section 2 of test';
    actual = 'abc'.replace('b', 'Z');
    expect = 'aZc';
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section 3 of test';
    actual = 'abc'.replace(undefined, 'Z');
    expect = 'abc'; // String(undefined) == 'undefined'; no replacement possible
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section 4 of test';
    actual = 'abc'.replace(null, 'Z');
    expect = 'abc'; // String(null) == 'null'; no replacement possible
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section 5 of test';
    actual = 'abc'.replace(true, 'Z');
    expect = 'abc'; // String(true) == 'true'; no replacement possible
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section 6 of test';
    actual = 'abc'.replace(false, 'Z');
    expect = 'abc'; // String(false) == 'false'; no replacement possible
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section 7 of test';
    actual = 'aa$aa'.replace('$', 'Z');
    expect = 'aaZaa'; // NOT 'aa$aaZ' as in ECMA Final Draft; see above
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section 8 of test';
    actual = 'abc'.replace('.*', 'Z');
    expect = 'abc';  // not 'Z' as in EMCA Final Draft
    array[item++] = new TestCase(SECTION, status, expect, actual);

    status = 'Section 9 of test';
    actual = 'abc'.replace('', 'Z');
    expect = 'Zabc';  // Still expect 'Zabc' for this
    array[item++] = new TestCase(SECTION, status, expect, actual);

    return array;
}
