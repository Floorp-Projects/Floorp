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

    var SECTION = "15.8.2.9";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Math.floor(x)";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,  "Math.floor.length",                    1,              Math.floor.length );
  /*thisError="no error";
    try{
        Math.floor();
    }catch(e:Error){
        thisError=(e.toString()).substring(0,26);
    }finally{//print(thisError);
        array[item++] = new TestCase( SECTION,   "Math.floor()","ArgumentError: Error #1063",thisError);
    }
    array[item++] = new TestCase( SECTION,  "Math.floor()",                         Number.NaN,     Math.floor() );*/
    array[item++] = new TestCase( SECTION,  "Math.floor(void 0)",                   Number.NaN,     Math.floor(void 0) );
    array[item++] = new TestCase( SECTION,  "Math.floor(null)",                     0,              Math.floor(null) );
    array[item++] = new TestCase( SECTION,  "Math.floor(true)",                     1,              Math.floor(true) );
    array[item++] = new TestCase( SECTION,  "Math.floor(false)",                    0,              Math.floor(false) );

    array[item++] = new TestCase( SECTION,  "Math.floor('1.1')",                    1,              Math.floor("1.1") );
    array[item++] = new TestCase( SECTION,  "Math.floor('-1.1')",                   -2,             Math.floor("-1.1") );
    array[item++] = new TestCase( SECTION,  "Math.floor('0.1')",                    0,              Math.floor("0.1") );
    array[item++] = new TestCase( SECTION,  "Math.floor('-0.1')",                   -1,             Math.floor("-0.1") );

    array[item++] = new TestCase( SECTION,  "Math.floor(NaN)",                      Number.NaN,     Math.floor(Number.NaN)  );
    array[item++] = new TestCase( SECTION,  "Math.floor(NaN)==-Math.ceil(-NaN)",    false,          Math.floor(Number.NaN) == -Math.ceil(-Number.NaN) );

    array[item++] = new TestCase( SECTION,  "Math.floor(0)",                        0,              Math.floor(0)           );
    array[item++] = new TestCase( SECTION,  "Math.floor(0)==-Math.ceil(-0)",        true,           Math.floor(0) == -Math.ceil(-0) );

    array[item++] = new TestCase( SECTION,  "Math.floor(-0)",           -0,                          Math.floor(-0)          );
    array[item++] = new TestCase( SECTION,  "Infinity/Math.floor(-0)",           -Infinity,         Infinity/Math.floor(-0)          );
    array[item++] = new TestCase( SECTION,  "Math.floor(-0)==-Math.ceil(0)",        true,           Math.floor(-0)== -Math.ceil(0) );

    array[item++] = new TestCase( SECTION,  "Math.floor(Infinity)",     Number.POSITIVE_INFINITY,   Math.floor(Number.POSITIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,  "Math.floor(Infinity)==-Math.ceil(-Infinity)",  true,   Math.floor(Number.POSITIVE_INFINITY) == -Math.ceil(Number.NEGATIVE_INFINITY) );

    array[item++] = new TestCase( SECTION,  "Math.floor(-Infinity)",    Number.NEGATIVE_INFINITY,   Math.floor(Number.NEGATIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,  "Math.floor(-Infinity)==-Math.ceil(Infinity)",  true,   Math.floor(Number.NEGATIVE_INFINITY) == -Math.ceil(Number.POSITIVE_INFINITY) );

    array[item++] = new TestCase( SECTION,  "Math.floor(0.0000001)",    0,                          Math.floor(0.0000001) );
    array[item++] = new TestCase( SECTION,  "Math.floor(0.0000001)==-Math.ceil(0.0000001)", true,   Math.floor(0.0000001)==-Math.ceil(-0.0000001) );

    array[item++] = new TestCase( SECTION,  "Math.floor(-0.0000001)",   -1,                         Math.floor(-0.0000001) );
    array[item++] = new TestCase( SECTION,  "Math.floor(0.0000001)==-Math.ceil(0.0000001)",  true,  Math.floor(-0.0000001)==-Math.ceil(0.0000001) );
    array[item++] = new TestCase( SECTION, "Math.floor(1)",          1,              Math.floor(1)   );
    array[item++] = new TestCase( SECTION, "Math.floor(-1)",          -1,            Math.floor(-1)   );
    array[item++] = new TestCase( SECTION, "Math.floor(-0.9)",        -1,            Math.floor(-0.9) );
    array[item++] = new TestCase( SECTION, "Infinity/Math.floor(-0.9)",  -Infinity,  Infinity/Math.floor(-0.9) );
    array[item++] = new TestCase( SECTION, "Math.floor(0.9 )",        0,             Math.floor( 0.9) );
    array[item++] = new TestCase( SECTION, "Math.floor(-1.1)",        -2,            Math.floor( -1.1));
    array[item++] = new TestCase( SECTION, "Math.floor( 1.1)",        1,             Math.floor(  1.1));

    array[item++] = new TestCase( SECTION, "Math.floor(Infinity)",   -Math.ceil(-Infinity),    Math.floor(Number.POSITIVE_INFINITY) );
    array[item++] = new TestCase( SECTION, "Math.floor(-Infinity)",  -Math.ceil(Infinity),     Math.floor(Number.NEGATIVE_INFINITY) );
    array[item++] = new TestCase( SECTION, "Math.floor(-Number.MIN_VALUE)",   -Math.ceil(Number.MIN_VALUE),     Math.floor(-Number.MIN_VALUE) );
    array[item++] = new TestCase( SECTION, "Math.floor(1)",          -Math.ceil(-1),        Math.floor(1)   );
    array[item++] = new TestCase( SECTION, "Math.floor(-1)",         -Math.ceil(1),         Math.floor(-1)   );
    array[item++] = new TestCase( SECTION, "Math.floor(-0.9)",       -Math.ceil(0.9),       Math.floor(-0.9) );
    array[item++] = new TestCase( SECTION, "Math.floor(0.9 )",       -Math.ceil(-0.9),      Math.floor( 0.9) );
    array[item++] = new TestCase( SECTION, "Math.floor(-1.1)",       -Math.ceil(1.1),       Math.floor( -1.1));
    array[item++] = new TestCase( SECTION, "Math.floor( 1.1)",       -Math.ceil(-1.1),      Math.floor(  1.1));
    array[item++] = new TestCase( SECTION, "Math.floor( .012345)",       0,      Math.floor(  .012345));
    array[item++] = new TestCase( SECTION, "Math.floor( .0012345)",       0,      Math.floor(.0012345));
    array[item++] = new TestCase( SECTION, "Math.floor( .00012345)",      0,      Math.floor(.00012345));
    array[item++] = new TestCase( SECTION, "Math.floor( .0000012345)",       0,      Math.floor(  .0000012345));
    array[item++] = new TestCase( SECTION, "Math.floor( .00000012345)",       0,      Math.floor(  .00000012345));
    array[item++] = new TestCase( SECTION, "Math.floor( 5.01)",       5,      Math.floor(  5.01));
    array[item++] = new TestCase( SECTION, "Math.floor( 5.001)",       5,      Math.floor(  5.001));
    array[item++] = new TestCase( SECTION, "Math.floor( 5.0001)",       5,      Math.floor(  5.0001));
    array[item++] = new TestCase( SECTION, "Math.floor( 5.00001)",       5,      Math.floor(  5.00001));
    array[item++] = new TestCase( SECTION, "Math.floor( 5.000001)",       5,      Math.floor(  5.000001));
    array[item++] = new TestCase( SECTION, "Math.floor( 5.0000001)",       5,      Math.floor(5.0000001));

    return ( array );
}
