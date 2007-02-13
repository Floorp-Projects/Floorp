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
    var SECTION = "15.4.2.2-1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "The Array Constructor:  new Array( len )";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,	"new Array(0)",             "",                 (new Array(0)).toString() );
    array[item++] = new TestCase( SECTION,	"typeof new Array(0)",      "object",           (typeof new Array(0)) );
    array[item++] = new TestCase( SECTION,	"(new Array(0)).length",    0,                  (new Array(0)).length );
    array[item++] = new TestCase( SECTION,	"(new Array(0)).toString", "function Function() {}",    ((new Array(0)).toString).toString() );

    array[item++] = new TestCase( SECTION,   "new Array(1)",            "",                 (new Array(1)).toString() );
    array[item++] = new TestCase( SECTION,   "new Array(1).length",     1,                  (new Array(1)).length );
    array[item++] = new TestCase( SECTION,   "(new Array(1)).toString", "function Function() {}",   ((new Array(1)).toString).toString() );

    array[item++] = new TestCase( SECTION,	"(new Array(-0)).length",                       0,  (new Array(-0)).length );
    array[item++] = new TestCase( SECTION,	"(new Array(0)).length",                        0,  (new Array(0)).length );

    array[item++] = new TestCase( SECTION,	"(new Array(10)).length",           10,         (new Array(10)).length );
    array[item++] = new TestCase( SECTION,	"(new Array('1')).length",          1,          (new Array('1')).length );
    array[item++] = new TestCase( SECTION,	"(new Array(1000)).length",         1000,       (new Array(1000)).length );
    array[item++] = new TestCase( SECTION,	"(new Array('1000')).length",       1,          (new Array('1000')).length );

    array[item++] = new TestCase( SECTION,	"(new Array(4294967295)).length",   ToUint32(4294967295),   (new Array(4294967295)).length );

    array[item++] = new TestCase( SECTION,	"(new Array('8589934592')).length", 1,                      (new Array("8589934592")).length );
    array[item++] = new TestCase( SECTION,	"(new Array('4294967296')).length", 1,                      (new Array("4294967296")).length );
    array[item++] = new TestCase( SECTION,	"(new Array(1073741824)).length",   ToUint32(1073741824),	(new Array(1073741824)).length );

    return ( array );
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
