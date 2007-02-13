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

    var SECTION = "15.4.4.4-1";
    var VERSION = "ECMA_1";
    startTest();
    var testcases = getTestCases();

    writeHeaderToLog( SECTION + " Array.prototype.reverse()");

    test();

function getTestCases() {
    var array = new Array();
    var item = 0;
    var ARR_PROTOTYPE = Array.prototype;

    array[item++] = new TestCase( SECTION, "Array.prototype.reverse.length",           0,      Array.prototype.reverse.length );
    array[item++] = new TestCase( SECTION, "delete Array.prototype.reverse.length",    false,  delete Array.prototype.reverse.length );
    array[item++] = new TestCase( SECTION, "delete Array.prototype.reverse.length; Array.prototype.reverse.length",    0, (delete Array.prototype.reverse.length, Array.prototype.reverse.length) );

    // length of array is 0
	var A;
    array[item++] = new TestCase(   SECTION,
                                    "var A = new Array();   A.reverse(); A.length",
                                    0,
                                    (A = new Array(),   A.reverse(), A.length ) );

	function CheckItems( R, A ) {
	    for ( var i = 0; i < R.length; i++ ) {
	        array[item++] = new TestCase(
	                                            SECTION,
	                                            "A["+i+ "]",
	                                            R[i],
	                                            A[i] );
	    }
	}

    return ( array );
}

/*function Object_1( value ) {
    this.array = value.split(",");
    this.length = this.array.length;
    for ( var i = 0; i < this.length; i++ ) {
        this[i] = this.array[i];
    }
    this.join = Array.prototype.reverse;
    this.getClass = Object.prototype.toString;
}*/
function Reverse( array ) {
    var r2 = array.length;
    var k = 0;
    var r3 = Math.floor( r2/2 );
    if ( r3 == k ) {
        return array;
    }

    for ( k = 0;  k < r3; k++ ) {
        var r6 = r2 - k - 1;
//        var r7 = String( k );
        var r7 = k;
        var r8 = String( r6 );

        var r9 = array[r7];
        var r10 = array[r8];

        array[r7] = r10;
        array[r8] = r9;
    }

    return array;
}
function Iterate( array ) {
    for ( var i = 0; i < array.length; i++ ) {
//        print( i+": "+ array[String(i)] );
    }
}

function Object_1( value ) {
    this.array = value.split(",");
    this.length = this.array.length;
    for ( var i = 0; i < this.length; i++ ) {
        this[i] = this.array[i];
    }
    this.reverse = Array.prototype.reverse;
    this.getClass = Object.prototype.toString;
}
