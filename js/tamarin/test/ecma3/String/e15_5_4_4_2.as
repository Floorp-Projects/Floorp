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
    var SECTION = "15.5.4.4-2";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "String.prototype.charAt";

    //writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

  /*array[item++] = new TestCase( SECTION,     "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(0)", "t",    (x = new Boolean(true), x.charAt=String.prototype.charAt,x.charAt(0)) );
    array[item++] = new TestCase( SECTION,     "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(1)", "r",    (x = new Boolean(true), x.charAt=String.prototype.charAt,x.charAt(1)) );
    array[item++] = new TestCase( SECTION,     "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(2)", "u",    (x = new Boolean(true), x.charAt=String.prototype.charAt,x.charAt(2)) );
    array[item++] = new TestCase( SECTION,     "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(3)", "e",    (x = new Boolean(true), x.charAt=String.prototype.charAt,x.charAt(3)) );
    array[item++] = new TestCase( SECTION,     "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(4)", "",     (x = new Boolean(true), x.charAt=String.prototype.charAt,x.charAt(4)) );
    array[item++] = new TestCase( SECTION,     "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(-1)", "",    (x = new Boolean(true), x.charAt=String.prototype.charAt,x.charAt(-1)) );

    array[item++] = new TestCase( SECTION,     "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(true)", "r",    (x = new Boolean(true), x.charAt=String.prototype.charAt,x.charAt(true)) );
    array[item++] = new TestCase( SECTION,     "x = new Boolean(true); x.charAt=String.prototype.charAt;x.charAt(false)", "t",    (x = new Boolean(true), x.charAt=String.prototype.charAt,x.charAt(false)) );*/

    array[item++] = new TestCase( SECTION,     "x = new String(); x.charAt(0)",    "",     (x=new String(),x.charAt(0)) );
    array[item++] = new TestCase( SECTION,     "x = new String(); x.charAt(1)",    "",     (x=new String(),x.charAt(1)) );
    array[item++] = new TestCase( SECTION,     "x = new String(); x.charAt(-1)",   "",     (x=new String(),x.charAt(-1)) );

    array[item++] = new TestCase( SECTION,     "x = new String(); x.charAt(NaN)",  "",     (x=new String(),x.charAt(Number.NaN)) );
    array[item++] = new TestCase( SECTION,     "x = new String(); x.charAt(Number.POSITIVE_INFINITY)",   "",     (x=new String(),x.charAt(Number.POSITIVE_INFINITY)) );
    array[item++] = new TestCase( SECTION,     "x = new String(); x.charAt(Number.NEGATIVE_INFINITY)",   "",     (x=new String(),x.charAt(Number.NEGATIVE_INFINITY)) );

    var MYOB = new MyObject(1234567890);
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyObject(1234567890), MYOB.charAt(0)",  "1",        (MYOB.charAt(0) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyObject(1234567890), MYOB.charAt(1)",  "2",        (MYOB.charAt(1) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyObject(1234567890), MYOB.charAt(2)",  "3",        (MYOB.charAt(2) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyObject(1234567890), MYOB.charAt(3)",  "4",        (MYOB.charAt(3) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyObject(1234567890), MYOB.charAt(4)",  "5",        (MYOB.charAt(4) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyObject(1234567890), MYOB.charAt(5)",  "6",        (MYOB.charAt(5) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyObject(1234567890), MYOB.charAt(6)",  "7",        (MYOB.charAt(6) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyObject(1234567890), MYOB.charAt(7)",  "8",        (MYOB.charAt(7) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyObject(1234567890), MYOB.charAt(8)",  "9",        (MYOB.charAt(8) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyObject(1234567890), MYOB.charAt(9)",  "0",        (MYOB.charAt(9) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyObject(1234567890), MYOB.charAt(10)",  "",       (MYOB.charAt(10) ) );

    array[item++] = new TestCase( SECTION,      "var MYOB = new MyObject(1234567890), MYOB.charAt(Math.PI)",  "4",        (MYOB = new MyObject(1234567890), MYOB.charAt(Math.PI) ) );

    // MyOtherObject.toString will return "[object Object]

    var MYOB = new MyOtherObject(1234567890); 
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(0)",  "[",        (MYOB.charAt(0) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(1)",  "o",        (MYOB.charAt(1) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(2)",  "b",        (MYOB.charAt(2) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(3)",  "j",        (MYOB.charAt(3) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(4)",  "e",        (MYOB.charAt(4) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(5)",  "c",        (MYOB.charAt(5) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(6)",  "t",        (MYOB.charAt(6) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(7)",  " ",        (MYOB.charAt(7) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(8)",  "O",        (MYOB.charAt(8) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(9)",  "b",        (MYOB.charAt(9) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(10)",  "j",        (MYOB.charAt(10) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(11)",  "e",        (MYOB.charAt(11) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(12)",  "c",        (MYOB.charAt(12) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(13)",  "t",        (MYOB.charAt(13) ) );
    array[item++] = new TestCase( SECTION,      "var MYOB = new MyOtherObject(1234567890), MYOB.charAt(14)",  "]",        (MYOB.charAt(14) ) );

    return (array );
}

function MyObject( value ) {
    this.value      = value;
    this.valueOf    = function() { return this.value; }
    this.toString   = function() { return this.value +'' }
    this.charAt     = String.prototype.charAt;
}
function MyOtherObject(value) {
    this.value      = value;
    this.valueOf    = function() { return this.value; }
    this.charAt     = String.prototype.charAt;
}
