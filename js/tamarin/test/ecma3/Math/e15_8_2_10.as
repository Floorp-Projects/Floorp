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

    var SECTION = "15.8.2.10";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Math.log(x)";
    var BUGNUMBER = "77391";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,   "Math.log.length",         1,              Math.log.length );
  /*thisError="no error";
    try{
        Math.log();
    }catch(e:Error){
        thisError=(e.toString()).substring(0,26);
    }finally{//print(thisError);
        array[item++] = new TestCase( SECTION,   "Math.log()","ArgumentError: Error #1063",thisError);
    }
    array[item++] = new TestCase( SECTION,   "Math.log()",              Number.NaN,     Math.log() );*/
    array[item++] = new TestCase( SECTION,   "Math.log(void 0)",        Number.NaN,     Math.log(void 0) );
    array[item++] = new TestCase( SECTION,   "Math.log(null)",          Number.NEGATIVE_INFINITY,   Math.log(null) );
    array[item++] = new TestCase( SECTION,   "Math.log(true)",          0,              Math.log(true) );
    array[item++] = new TestCase( SECTION,   "Math.log(false)",         -Infinity,      Math.log(false) );
    array[item++] = new TestCase( SECTION,   "Math.log('0')",           -Infinity,      Math.log('0') );
    array[item++] = new TestCase( SECTION,   "Math.log('1')",           0,              Math.log('1') );
    array[item++] = new TestCase( SECTION,   "Math.log('Infinity')",    Infinity,       Math.log("Infinity") );

    array[item++] = new TestCase( SECTION,   "Math.log(NaN)",           Number.NaN,     Math.log(Number.NaN) );
    array[item++] = new TestCase( SECTION,   "Math.log(-0.0000001)",    Number.NaN,     Math.log(-0.000001)  );
    array[item++] = new TestCase( SECTION,   "Math.log(0.0000001)",    -13.815510557964274,     Math.log(0.000001)  );
    array[item++] = new TestCase( SECTION,   "Math.log(-1)",            Number.NaN,     Math.log(-1)        );
    array[item++] = new TestCase( SECTION,   "Math.log(0)",             Number.NEGATIVE_INFINITY,   Math.log(0) );
    array[item++] = new TestCase( SECTION,   "Math.log(-0)",            Number.NEGATIVE_INFINITY,   Math.log(-0));
    array[item++] = new TestCase( SECTION,   "Math.log(1)",             0,              Math.log(1) );
    array[item++] = new TestCase( SECTION,   "Math.log(2)",0.6931471805599453,              Math.log(2) );
    array[item++] = new TestCase( SECTION,   "Math.log(3)", 1.0986122886681098,              Math.log(3) );
    array[item++] = new TestCase( SECTION,   "Math.log(4)",1.3862943611198906,              Math.log(4) );
    array[item++] = new TestCase( SECTION,   "Math.log(Infinity)",      Number.POSITIVE_INFINITY,   Math.log(Number.POSITIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,   "Math.log(-Infinity)",     Number.NaN,     Math.log(Number.NEGATIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,   "Math.log(10)",2.302585092994046,              Math.log(10) );
    array[item++] = new TestCase( SECTION,   "Math.log(100)",4.605170185988092,              Math.log(100) );
    array[item++] = new TestCase( SECTION,   "Math.log(100000)",11.512925464970229,              Math.log(100000) );
    array[item++] = new TestCase( SECTION,   "Math.log(300000)",12.611537753638338,              Math.log(300000) );
    return ( array );
}
