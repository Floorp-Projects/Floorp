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

    var SECTION = "15.8.2.15";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Math.round(x)";
    var BUGNUMBER="331411";

    var EXCLUDE = "true";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,  "Math.round.length",   1,               Math.round.length );
  /*thisError="no error";
    try{
        Math.round();
    }catch(e:Error){
        thisError=(e.toString()).substring(0,26);
    }finally{//print(thisError);
        array[item++] = new TestCase( SECTION,   "Math.round()","ArgumentError: Error #1063",thisError);
    }
    array[item++] = new TestCase( SECTION,  "Math.round()",         Number.NaN,     Math.round() );*/
    array[item++] = new TestCase( SECTION,  "Math.round(null)",     0,              Math.round(0) );
    array[item++] = new TestCase( SECTION,  "Math.round(void 0)",   Number.NaN,     Math.round(void 0) );
    array[item++] = new TestCase( SECTION,  "Math.round(true)",     1,              Math.round(true) );
    array[item++] = new TestCase( SECTION,  "Math.round(false)",    0,              Math.round(false) );
    array[item++] = new TestCase( SECTION,  "Math.round('.99999')",  1,              Math.round('.99999') );
    array[item++] = new TestCase( SECTION,  "Math.round('12345e-2')",  123,          Math.round('12345e-2') );

    array[item++] = new TestCase( SECTION,  "Math.round(NaN)",      Number.NaN,     Math.round(Number.NaN) );
    array[item++] = new TestCase( SECTION,  "Math.round(0)",        0,              Math.round(0) );
    array[item++] = new TestCase( SECTION,  "Math.round(-0)",        0,            Math.round(-0));
    array[item++] = new TestCase( SECTION,  "Infinity/Math.round(-0)",  Infinity,  Infinity/Math.round(-0) );

    array[item++] = new TestCase( SECTION,  "Math.round(Infinity)", Number.POSITIVE_INFINITY,   Math.round(Number.POSITIVE_INFINITY));
    array[item++] = new TestCase( SECTION,  "Math.round(-Infinity)",Number.NEGATIVE_INFINITY,       Math.round(Number.NEGATIVE_INFINITY));
    array[item++] = new TestCase( SECTION,  "Math.round(0.49)",     0,              Math.round(0.49));
    array[item++] = new TestCase( SECTION,  "Math.round(0.5)",      1,              Math.round(0.5));
    array[item++] = new TestCase( SECTION,  "Math.round(0.51)",     1,              Math.round(0.51));

    array[item++] = new TestCase( SECTION,  "Math.round(-0.49)",    0,             Math.round(-0.49));
    array[item++] = new TestCase( SECTION,  "Math.round(-0.5)",     0,             Math.round(-0.5));
    array[item++] = new TestCase( SECTION,  "Infinity/Math.round(-0.49)",    Infinity,             Infinity/Math.round(-0.49));
    array[item++] = new TestCase( SECTION,  "Infinity/Math.round(-0.5)",     Infinity,             Infinity/Math.round(-0.5));

    array[item++] = new TestCase( SECTION,  "Math.round(-0.51)",    -1,             Math.round(-0.51));
    array[item++] = new TestCase( SECTION,  "Math.round(3.5)",      4,              Math.round(3.5));
    array[item++] = new TestCase( SECTION,  "Math.round(-3.5)",     -3,             Math.round(-3));

    return ( array );
}
