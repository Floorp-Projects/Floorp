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


    var SECTION = "15.4.4.5-3";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Array.prototype.sort(comparefn)";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = new Array();
    getTestCases();
    test("no actual"); // don't want the actual results printed to output

function getTestCases() {
    var array = new Array();

    array[array.length] = new Date( TIME_2000 * Math.PI );
    array[array.length] = new Date( TIME_2000 * 10 );
    array[array.length] = new Date( TIME_1900 + TIME_1900  );
    array[array.length] = new Date(0);
    array[array.length] = new Date( TIME_2000 );
    array[array.length] = new Date( TIME_1900 + TIME_1900 +TIME_1900 );
    array[array.length] = new Date( TIME_1900 * Math.PI );
    array[array.length] = new Date( TIME_1900 * 10 );
    array[array.length] = new Date( TIME_1900 );
    array[array.length] = new Date( TIME_2000 + TIME_2000 );
    array[array.length] = new Date( 1899, 0, 1 );
    array[array.length] = new Date( 2000, 1, 29 );
    array[array.length] = new Date( 2000, 0, 1 );
    array[array.length] = new Date( 1999, 11, 31 );

   var testarr1 = new Array()
   clone( array, testarr1 );
   testarr1.sort( comparefn1 );

   var testarr2 = new Array()
   clone( array, testarr2 );
   testarr2.sort( comparefn2 );

    var testarr3 = new Array();
    clone( array, testarr3 );
    testarr3.sort( comparefn3 );

    // when there's no sort function, sort sorts by the toString value of Date.

   var testarr4 = new Array()
   clone( array, testarr4 );
   testarr4.sort();

    var realarr = new Array();
    clone( array, realarr );
    realarr.sort( realsort );

    var stringarr = new Array();
    clone( array, stringarr );
    stringarr.sort( stringsort );

    for ( var i = 0, tc = 0; i < array.length; i++, tc++) {
        testcases[tc] = new TestCase(
            SECTION,
            "testarr1["+i+"]",
            realarr[i],
            testarr1[i] );
    }

    for ( var i=0; i < array.length; i++,  tc++) {
        testcases[tc] = new TestCase(
            SECTION,
            "testarr2["+i+"]",
            realarr[i],
            testarr2[i] );
    }

    for ( var i=0; i < array.length; i++,  tc++) {
        testcases[tc] = new TestCase(
            SECTION,
            "testarr3["+i+"]",
            realarr[i],
            testarr3[i] );
    }

    for ( var i=0; i < array.length; i++,  tc++) {
        testcases[tc] = new TestCase(
            SECTION,
            "testarr4["+i+"]",
            stringarr[i].toString(),
            testarr4[i].toString() );
    }

}
function comparefn1( x, y ) {
    return x - y;
}
function comparefn2( x, y ) {
    return x.valueOf() - y.valueOf();
}
function realsort( x, y ) {
    return ( x.valueOf() == y.valueOf() ? 0 : ( x.valueOf() > y.valueOf() ? 1 : -1 ) );
}
function comparefn3( x, y ) {
    return ( x == y ? 0 : ( x > y ? 1: -1 ) );
}
function clone( source, target ) {
    for (var i = 0; i < source.length; i++ ) {
        target[i] = source[i];
    }
}
function stringsort( x, y ) {
    for ( var i = 0; i < x.toString().length; i++ ) {
        var d = (x.toString()).charCodeAt(i) - (y.toString()).charCodeAt(i);
        if ( d > 0 ) {
            return 1;
        } else {
            if ( d < 0 ) {
                return -1;
            } else {
                continue;
            }
        }

        var d = x.length - y.length;

        if  ( d > 0 ) {
            return 1;
        } else {
            if ( d < 0 ) {
                return -1;
            }
        }
   }
    return 0;
}
