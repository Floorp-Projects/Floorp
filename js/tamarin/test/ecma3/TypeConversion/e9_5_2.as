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
    var SECTION = "9.5-2";
    var VERSION = "ECMA_1";
    startTest();
    var testcases = getTestCases();

    writeHeaderToLog( SECTION + " ToInt32");
    test();

function ToInt32( n ) {
    n = Number( n );
    var sign = ( n < 0 ) ? -1 : 1;

    if ( Math.abs( n ) == 0 || Math.abs( n ) == Number.POSITIVE_INFINITY) {
        return 0;
    }

    n = (sign * Math.floor( Math.abs(n) )) % Math.pow(2,32);
    if ( sign == -1 ) {
        n = ( n < -Math.pow(2,31) ) ? n + Math.pow(2,32) : n;
    } else{
        n = ( n >= Math.pow(2,31) ) ? n - Math.pow(2,32) : n;
    }

    return ( n );
}
function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,   "0 << 0",                        0,              0 << 0 );
    array[item++] = new TestCase( SECTION,   "-0 << 0",                       0,              -0 << 0 );
    array[item++] = new TestCase( SECTION,   "Infinity << 0",                 0,              "Infinity" << 0 );
    array[item++] = new TestCase( SECTION,   "-Infinity << 0",                0,              "-Infinity" << 0 );
    array[item++] = new TestCase( SECTION,   "Number.POSITIVE_INFINITY << 0", 0,              Number.POSITIVE_INFINITY << 0 );
    array[item++] = new TestCase( SECTION,   "Number.NEGATIVE_INFINITY << 0", 0,              Number.NEGATIVE_INFINITY << 0 );
    array[item++] = new TestCase( SECTION,   "Number.NaN << 0",               0,              Number.NaN << 0 );

    array[item++] = new TestCase( SECTION,   "Number.MIN_VALUE << 0",         0,              Number.MIN_VALUE << 0 );
    array[item++] = new TestCase( SECTION,   "-Number.MIN_VALUE << 0",        0,              -Number.MIN_VALUE << 0 );
    array[item++] = new TestCase( SECTION,   "0.1 << 0",                      0,              0.1 << 0 );
    array[item++] = new TestCase( SECTION,   "-0.1 << 0",                     0,              -0.1 << 0 );
    array[item++] = new TestCase( SECTION,   "1 << 0",                        1,              1 << 0 );
    array[item++] = new TestCase( SECTION,   "1.1 << 0",                      1,              1.1 << 0 );
    array[item++] = new TestCase( SECTION,   "-1 << 0",                     ToInt32(-1),             -1 << 0 );


    array[item++] = new TestCase( SECTION,   "2147483647 << 0",     ToInt32(2147483647),    2147483647 << 0 );
    array[item++] = new TestCase( SECTION,   "2147483648 << 0",     ToInt32(2147483648),    2147483648 << 0 );
    array[item++] = new TestCase( SECTION,   "2147483649 << 0",     ToInt32(2147483649),    2147483649 << 0 );

    array[item++] = new TestCase( SECTION,   "(Math.pow(2,31)-1) << 0", ToInt32(2147483647),    (Math.pow(2,31)-1) << 0 );
    array[item++] = new TestCase( SECTION,   "Math.pow(2,31) << 0",     ToInt32(2147483648),    Math.pow(2,31) << 0 );
    array[item++] = new TestCase( SECTION,   "(Math.pow(2,31)+1) << 0", ToInt32(2147483649),    (Math.pow(2,31)+1) << 0 );

    array[item++] = new TestCase( SECTION,   "(Math.pow(2,32)-1) << 0",   ToInt32(4294967295),    (Math.pow(2,32)-1) << 0 );
    array[item++] = new TestCase( SECTION,   "(Math.pow(2,32)) << 0",     ToInt32(4294967296),    (Math.pow(2,32)) << 0 );
    array[item++] = new TestCase( SECTION,   "(Math.pow(2,32)+1) << 0",   ToInt32(4294967297),    (Math.pow(2,32)+1) << 0 );

    array[item++] = new TestCase( SECTION,   "4294967295 << 0",     ToInt32(4294967295),    4294967295 << 0 );
    array[item++] = new TestCase( SECTION,   "4294967296 << 0",     ToInt32(4294967296),    4294967296 << 0 );
    array[item++] = new TestCase( SECTION,   "4294967297 << 0",     ToInt32(4294967297),    4294967297 << 0 );

    array[item++] = new TestCase( SECTION,   "'2147483647' << 0",   ToInt32(2147483647),    '2147483647' << 0 );
    array[item++] = new TestCase( SECTION,   "'2147483648' << 0",   ToInt32(2147483648),    '2147483648' << 0 );
    array[item++] = new TestCase( SECTION,   "'2147483649' << 0",   ToInt32(2147483649),    '2147483649' << 0 );

    array[item++] = new TestCase( SECTION,   "'4294967295' << 0",   ToInt32(4294967295),    '4294967295' << 0 );
    array[item++] = new TestCase( SECTION,   "'4294967296' << 0",   ToInt32(4294967296),    '4294967296' << 0 );
    array[item++] = new TestCase( SECTION,   "'4294967297' << 0",   ToInt32(4294967297),    '4294967297' << 0 );

    array[item++] = new TestCase( SECTION,   "-2147483647 << 0",    ToInt32(-2147483647),   -2147483647	<< 0 );
    array[item++] = new TestCase( SECTION,   "-2147483648 << 0",    ToInt32(-2147483648),   -2147483648 << 0 );
    array[item++] = new TestCase( SECTION,   "-2147483649 << 0",    ToInt32(-2147483649),   -2147483649 << 0 );

    array[item++] = new TestCase( SECTION,   "-4294967295 << 0",    ToInt32(-4294967295),   -4294967295 << 0 );
    array[item++] = new TestCase( SECTION,   "-4294967296 << 0",    ToInt32(-4294967296),   -4294967296 << 0 );
    array[item++] = new TestCase( SECTION,   "-4294967297 << 0",    ToInt32(-4294967297),   -4294967297 << 0 );

    /*
     * Numbers between 2^31 and 2^32 will have a negative ToInt32 per ECMA (see step 5 of introduction)
     * (These are by stevechapel@earthlink.net; cf. http://bugzilla.mozilla.org/show_bug.cgi?id=120083)
     */
    array[item++] = new TestCase( SECTION,   "2147483648.25 << 0",  ToInt32(2147483648.25),   2147483648.25 << 0 );
    array[item++] = new TestCase( SECTION,   "2147483648.5 << 0",   ToInt32(2147483648.5),    2147483648.5 << 0 );
    array[item++] = new TestCase( SECTION,   "2147483648.75 << 0",  ToInt32(2147483648.75),   2147483648.75 << 0 );
    array[item++] = new TestCase( SECTION,   "4294967295.25 << 0",  ToInt32(4294967295.25),   4294967295.25 << 0 );
    array[item++] = new TestCase( SECTION,   "4294967295.5 << 0",   ToInt32(4294967295.5),    4294967295.5 << 0 );
    array[item++] = new TestCase( SECTION,   "4294967295.75 << 0",  ToInt32(4294967295.75),   4294967295.75 << 0 );
    array[item++] = new TestCase( SECTION,   "3000000000.25 << 0",  ToInt32(3000000000.25),   3000000000.25 << 0 );
    array[item++] = new TestCase( SECTION,   "3000000000.5 << 0",   ToInt32(3000000000.5),    3000000000.5 << 0 );
    array[item++] = new TestCase( SECTION,   "3000000000.75 << 0",  ToInt32(3000000000.75),   3000000000.75 << 0 );

    /*
     * Numbers between - 2^31 and - 2^32 
     */
    array[item++] = new TestCase( SECTION,   "-2147483648.25 << 0",  ToInt32(-2147483648.25),   -2147483648.25 << 0 );
    array[item++] = new TestCase( SECTION,   "-2147483648.5 << 0",   ToInt32(-2147483648.5),    -2147483648.5 << 0 );
    array[item++] = new TestCase( SECTION,   "-2147483648.75 << 0",  ToInt32(-2147483648.75),   -2147483648.75 << 0 );
    array[item++] = new TestCase( SECTION,   "-4294967295.25 << 0",  ToInt32(-4294967295.25),   -4294967295.25 << 0 );
    array[item++] = new TestCase( SECTION,   "-4294967295.5 << 0",   ToInt32(-4294967295.5),    -4294967295.5 << 0 );
    array[item++] = new TestCase( SECTION,   "-4294967295.75 << 0",  ToInt32(-4294967295.75),   -4294967295.75 << 0 );
    array[item++] = new TestCase( SECTION,   "-3000000000.25 << 0",  ToInt32(-3000000000.25),   -3000000000.25 << 0 );
    array[item++] = new TestCase( SECTION,   "-3000000000.5 << 0",   ToInt32(-3000000000.5),    -3000000000.5 << 0 );
    array[item++] = new TestCase( SECTION,   "-3000000000.75 << 0",  ToInt32(-3000000000.75),   -3000000000.75 << 0 );

    return ( array );
}
