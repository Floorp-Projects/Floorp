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

    var SECTION = "15.4.4.3";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Array.prototype.toLocaleString";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,  "Array.prototype.toLocaleString.length",  0,  Array.prototype.toLocaleString.length );

    array[item++] = new TestCase( SECTION,  "(new Array()).toLocaleString()",     "",     (new Array()).toLocaleString() );
    array[item++] = new TestCase( SECTION,  "(new Array(2)).toLocaleString()",    ",",    (new Array(2)).toLocaleString() );
    array[item++] = new TestCase( SECTION,  "(new Array(0,1)).toLocaleString()",  "0,1",  (new Array(0,1)).toLocaleString() );
    array[item++] = new TestCase( SECTION,  "(new Array( Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY)).toLocaleString()",  "NaN,Infinity,-Infinity",   (new Array( Number.NaN, Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY)).toLocaleString() );
    array[item++] = new TestCase( SECTION,  "(new Array(Boolean(1), Boolean(0))).toLocaleString()", "[object Boolean],[object Boolean]" ,   (new Array(Boolean(1), Boolean(0))).toLocaleString());
    array[item++] = new TestCase( SECTION,  "(new Array(void 0,null)).toLocaleString()",    ",",                    (new Array(void 0,null)).toLocaleString() );

    var EXPECT_STRING = "";
    var MYARR = new Array();

    for ( var i = -50; i < 50; i+= 0.25 ) {
        MYARR[MYARR.length] = i;
        EXPECT_STRING += i +",";
    }

    EXPECT_STRING = EXPECT_STRING.substring( 0, EXPECT_STRING.length -1 );

    array[item++] = new TestCase( SECTION, "MYARR.toLocaleString()",  EXPECT_STRING,  MYARR.toLocaleString() );

    var MYARR2 = [0,1,2,3,4,5,6,7,8,9]

   array[item++] = new TestCase( SECTION, "MYARR2.toLocaleString()",  "0,1,2,3,4,5,6,7,8,9",  MYARR2.toLocaleString() );


   var MYARRARR = [new Array(1,2,3),new Array(4,5,6)]

   array[item++] = new TestCase( SECTION, "MYARRARR.toLocaleString()",  "1,2,3,4,5,6",MYARRARR.toLocaleString() );

   var obj;
   var MYUNDEFARR = [obj];

   array[item++] = new TestCase( SECTION, "MYUNDEFARR.toLocaleString()",  "",MYUNDEFARR.toLocaleString() );

   var MYNULLARR = [null]

   array[item++] = new TestCase( SECTION, "MYNULLARR.toLocaleString()",  "",MYNULLARR.toLocaleString() );

   var MYNULLARR2 = new Array(null);

   array[item++] = new TestCase( SECTION, "MYNULLARR2.toLocaleString()",  "",MYNULLARR2.toLocaleString() );

   var MyAllArray = new Array(new String('string'),new Array(1,2,3),new Number(100000),Boolean(0),Number.MAX_VALUE)

   array[item++] = new TestCase( SECTION, "MyAllArray.toLocaleString()",  "[object String],1,2,3,100000,[object Boolean],1.79769313486231e+308",MyAllArray.toLocaleString() );



    return ( array );
}
