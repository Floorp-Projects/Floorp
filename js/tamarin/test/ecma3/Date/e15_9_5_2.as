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
 * The Original Code is [Open Source Virtual Machine.].
 *
 * The Initial Developer of the Original Code is
 * Adobe System Incorporated.
 * Portions created by the Initial Developer are Copyright (C) 2005-2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adobe AS3 Team
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

    var SECTION = "15.9.5.2";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Date.prototype.toString";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;    

    array[item++] = new TestCase( SECTION,
                                    "Date.prototype.toString.length",
                                    0,
                                    Date.prototype.toString.length );

    var now = new Date();

    // can't test the content of the string, but can verify that the string is
    // parsable by Date.parse

    array[item++] = new TestCase( SECTION,
                                    "Math.abs(Date.parse(now.toString()) - now.valueOf()) < 1000",
                                    true,
                                    Math.abs(Date.parse(now.toString()) - now.valueOf()) < 1000 );

    array[item++] = new TestCase( SECTION,
                                    "typeof now.toString()",
                                    "string",
                                    typeof now.toString() );
    // 1970

    TZ_ADJUST = TZ_DIFF * msPerHour;

    array[item++] = new TestCase( SECTION,
                                    "Date.parse( (new Date(0)).toString() )",
                                    0,
                                    Date.parse( (new Date(0)).toString() ) )

    array[item++] = new TestCase( SECTION,
                                    "Date.parse( (new Date(+TZ_ADJUST+)).toString() )",
                                    TZ_ADJUST,
                                    Date.parse( (new Date(TZ_ADJUST)).toString() ) )

    // 1900
    array[item++] = new TestCase( SECTION,
                                    "Date.parse( (new Date(+TIME_1900+).toString() )",
                                    TIME_1900,
                                    Date.parse( (new Date(TIME_1900)).toString() ) )

    array[item++] = new TestCase( SECTION,
                                    "Date.parse( (new Date(+TIME_1900 -TZ_ADJUST+)).toString() )",
                                    TIME_1900 -TZ_ADJUST,
                                    Date.parse( (new Date(TIME_1900 -TZ_ADJUST)).toString() ) )

    // 2000
    array[item++] = new TestCase( SECTION,
                                    "Date.parse( (new Date(+TIME_2000+)).toString() )",
                                    TIME_2000,
                                    Date.parse( (new Date(TIME_2000)).toString() ) )

    array[item++] = new TestCase( SECTION,
                                    "Date.parse( (new Date(+TIME_2000 -TZ_ADJUST+)).toString() )",
                                    TIME_2000 -TZ_ADJUST,
                                    Date.parse( (new Date(TIME_2000 -TZ_ADJUST)).toString() ) )

    // 29 Feb 2000

    var UTC_29_FEB_2000 = TIME_2000 + 31*msPerDay + 28*msPerDay;
    array[item++] = new TestCase( SECTION,
                                    "Date.parse( (new Date(+UTC_29_FEB_2000+)).toString() )",
                                    UTC_29_FEB_2000,
                                    Date.parse( (new Date(UTC_29_FEB_2000)).toString() ) )

    array[item++] = new TestCase( SECTION,
                                    "Date.parse( (new Date(+(UTC_29_FEB_2000-1000)+)).toString() )",
                                    UTC_29_FEB_2000-1000,
                                    Date.parse( (new Date(UTC_29_FEB_2000-1000)).toString() ) )


    array[item++] = new TestCase( SECTION,
                                    "Date.parse( (new Date(+(UTC_29_FEB_2000-TZ_ADJUST)+)).toString() )",
                                    UTC_29_FEB_2000-TZ_ADJUST,
                                    Date.parse( (new Date(UTC_29_FEB_2000-TZ_ADJUST)).toString() ) )
    // 2O05

    var UTC_1_JAN_2005 = TIME_2000 + TimeInYear(2000) + TimeInYear(2001) +
    TimeInYear(2002) + TimeInYear(2003) + TimeInYear(2004);
    array[item++] = new TestCase( SECTION,
                                    "Date.parse( (new Date(+UTC_1_JAN_2005+)).toString() )",
                                    UTC_1_JAN_2005,
                                    Date.parse( (new Date(UTC_1_JAN_2005)).toString() ) )

    array[item++] = new TestCase( SECTION,
                                    "Date.parse( (new Date(+(UTC_1_JAN_2005-1000)+)).toString() )",
                                    UTC_1_JAN_2005-1000,
                                    Date.parse( (new Date(UTC_1_JAN_2005-1000)).toString() ) )

    array[item++] = new TestCase( SECTION,
                                    "Date.parse( (new Date(+(UTC_1_JAN_2005-TZ_ADJUST)+)).toString() )",
                                    UTC_1_JAN_2005-TZ_ADJUST,
                                    Date.parse( (new Date(UTC_1_JAN_2005-TZ_ADJUST)).toString() ) )
    return array;
}
