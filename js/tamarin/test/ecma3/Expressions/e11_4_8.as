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

    var SECTION = "e11_4_8";
    var VERSION = "ECMA_1";
    startTest();
    var testcases = getTestCases();

    writeHeaderToLog( SECTION + " Bitwise Not operator");
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    for ( var i = 0; i < 35; i++ ) {
        var p = Math.pow(2,i);

        array[item++] = new TestCase( SECTION, "~"+p,   Not(p),     ~p );

    }
    for ( i = 0; i < 35; i++ ) {
        var p = -Math.pow(2,i);

        array[item++] = new TestCase( SECTION, "~"+p,   Not(p),     ~p );

    }

    return ( array );
}
function ToInteger( n ) {
    n = Number( n );
    var sign = ( n < 0 ) ? -1 : 1;

    if ( n != n ) {
        return 0;
    }
    if ( Math.abs( n ) == 0 || Math.abs( n ) == Number.POSITIVE_INFINITY ) {
        return n;
    }
    return ( sign * Math.floor(Math.abs(n)) );
}
function ToInt32( n ) {
    n = Number( n );
    var sign = ( n < 0 ) ? -1 : 1;

    if ( Math.abs( n ) == 0 || Math.abs( n ) == Number.POSITIVE_INFINITY) {
        return 0;
    }

    n = (sign * Math.floor( Math.abs(n) )) % Math.pow(2,32);
    n = ( n >= Math.pow(2,31) ) ? n - Math.pow(2,32) : n;

    return ( n );
}
function ToUint32( n ) {
    n = Number( n );
    var sign = ( n < 0 ) ? -1 : 1;

    if ( Math.abs( n ) == 0 || Math.abs( n ) == Number.POSITIVE_INFINITY) {
        return 0;
    }
    n = sign * Math.floor( Math.abs(n) )

    n = n % Math.pow(2,32);

    if ( n < 0 ){
        n += Math.pow(2,32);
    }

    return ( n );
}
function ToUint16( n ) {
    var sign = ( n < 0 ) ? -1 : 1;

    if ( Math.abs( n ) == 0 || Math.abs( n ) == Number.POSITIVE_INFINITY) {
        return 0;
    }

    n = ( sign * Math.floor( Math.abs(n) ) ) % Math.pow(2,16);

    if (n <0) {
        n += Math.pow(2,16);
    }

    return ( n );
}
function Mask( b, n ) {
    b = ToUint32BitString( b );
    b = b.substring( b.length - n );
    b = ToUint32Decimal( b );
    return ( b );
}
function ToUint32BitString( n ) {
    var b = "";
    for ( p = 31; p >=0; p-- ) {
        if ( n >= Math.pow(2,p) ) {
            b += "1";
            n -= Math.pow(2,p);
        } else {
            b += "0";
        }
    }
    return b;
}
function ToInt32BitString( n ) {
    var b = "";
    var sign = ( n < 0 ) ? -1 : 1;

    b += ( sign == 1 ) ? "0" : "1";

    for ( p = 30; p >=0; p-- ) {
        if ( (sign == 1 ) ? sign * n >= Math.pow(2,p) : sign * n > Math.pow(2,p) ) {
            b += ( sign == 1 ) ? "1" : "0";
            n -= sign * Math.pow( 2, p );
        } else {
            b += ( sign == 1 ) ? "0" : "1";
        }
    }

    return b;
}
function ToInt32Decimal( bin ) {
    var r = 0;
    var sign;

    if ( Number(bin.charAt(0)) == 0 ) {
        sign = 1;
        r = 0;
    } else {
        sign = -1;
        r = -(Math.pow(2,31));
    }

    for ( var j = 0; j < 31; j++ ) {
        r += Math.pow( 2, j ) * Number(bin.charAt(31-j));
    }

    return r;
}
function ToUint32Decimal( bin ) {
    var r = 0;

    for ( l = bin.length; l < 32; l++ ) {
        bin = "0" + bin;
    }

    for ( j = 0; j < 31; j++ ) {
        r += Math.pow( 2, j ) * Number(bin.charAt(31-j));
    }

    return r;
}
function Not( n ) {
    n = ToInt32(n);
    n = ToInt32BitString(n);

    r = ""

    for( var l = 0; l < n.length; l++  ) {
        r += ( n.charAt(l) == "0" ) ? "1" : "0";
    }

    n = ToInt32Decimal(r);

    return n;
}
