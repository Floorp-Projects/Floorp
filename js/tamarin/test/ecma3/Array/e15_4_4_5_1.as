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


    var SECTION = "15.4.4.5-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Array.prototype.sort(comparefn)";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = new Array();
    getTestCases();
    test();

function getTestCases() {
    var S = new Array();
    var item = 0;
	var A;

    // array is empty.
    S[item++] = (A = new Array());

    // array contains one item
    S[item++] = (A = new Array( true ));

    // length of array is 2
    S[item++] = (A = new Array( true, false, new Boolean(true), new Boolean(false), 'true', 'false' ) );

    S[item++] = (A = new Array(), A[3] = 'undefined', A[6] = null, A[8] = 'null', A[0] = void 0, A);

    /*

    S[item] = "var A = new Array( ";

    var limit = 0x0061;
    for ( var i = 0x007A; i >= limit; i-- ) {
        S[item] += "\'"+ String.fromCharCode(i) +"\'" ;
        if ( i > limit ) {
            S[item] += ",";
        }
    }
    */

    S[item] = (A = new Array(( 0x007A - 0x0061) + 1) );

    var limit = 0x0061;
    var index = 0;
    for ( var i = 0x007A; i >= limit; i-- ) {
        A[index++] = String.fromCharCode(i);
    }

    item++;

    for ( var i = 0; i < S.length; i++ ) {
        CheckItems( S[i] );
    }
}
function CheckItems( S ) {
    var A = S;
    var E = Sort( A );

    testcases[testcases.length] = new TestCase(   SECTION,
                                    S +";  A.sort(); A.length",
                                    E.length,
                                    (A = S, A.sort(), A.length) );

    for ( var i = 0; i < E.length; i++ ) {
        testcases[testcases.length] = new TestCase(
                                            SECTION,
                                            "A["+i+ "].toString()",
                                            E[i] +"",
                                            A[i] +"");

        if ( A[i] == void 0 && typeof A[i] == "undefined" ) {
            testcases[testcases.length] = new TestCase(
                                            SECTION,
                                            "typeof A["+i+ "]",
                                            typeof E[i],
                                            typeof A[i] );
        }
    }
}
function Sort( a ) {
    for ( var i = 0; i < a.length; i++ ) {
        for ( var j = i+1; j < a.length; j++ ) {
            var lo = a[i];
            var hi = a[j];
            var c = Compare( lo, hi );
            if ( c == 1 ) {
                a[i] = hi;
                a[j] = lo;
            }
        }
    }
    return a;
}
function Compare( x, y ) {
    if ( x == void 0 && y == void 0  && typeof x == "undefined" && typeof y == "undefined" ) {
        return +0;
    }
    if ( x == void 0  && typeof x == "undefined" ) {
        return 1;
    }
    if ( y == void 0 && typeof y == "undefined" ) {
        return -1;
    }
    x = String(x);
    y = String(y);
    if ( x < y ) {
        return -1;
    }
    if ( x > y ) {
        return 1;
    }
    return 0;
}
