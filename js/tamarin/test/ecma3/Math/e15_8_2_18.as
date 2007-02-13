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

    var SECTION = "15.8.2.18";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Math.tan(x)";
    var EXCLUDE = "true";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,  "Math.tan.length",          1,              Math.tan.length );
  /*thisError="no error";
    try{
        Math.tan();
    }catch(e:Error){
        thisError=(e.toString()).substring(0,26);
    }finally{//print(thisError);
        array[item++] = new TestCase( SECTION,   "Math.tan()","ArgumentError: Error #1063",thisError);
    }
    array[item++] = new TestCase( SECTION,  "Math.tan()",               Number.NaN,      Math.tan() );*/
    array[item++] = new TestCase( SECTION,  "Math.tan(void 0)",         Number.NaN,     Math.tan(void 0));
    array[item++] = new TestCase( SECTION,  "Math.tan(null)",           0,              Math.tan(null) );
    array[item++] = new TestCase( SECTION,  "Math.tan(false)",          0,              Math.tan(false) );

    array[item++] = new TestCase( SECTION,  "Math.tan(NaN)",            Number.NaN,     Math.tan(Number.NaN) );
    array[item++] = new TestCase( SECTION,  "Math.tan(0)",              0,	            Math.tan(0));
    array[item++] = new TestCase( SECTION,  "Math.tan(-0)",             -0,         	Math.tan(-0));
    array[item++] = new TestCase( SECTION,  "Math.tan(Infinity)",       Number.NaN,     Math.tan(Number.POSITIVE_INFINITY));
    array[item++] = new TestCase( SECTION,  "Math.tan(-Infinity)",      Number.NaN,     Math.tan(Number.NEGATIVE_INFINITY));
    array[item++] = new TestCase( SECTION,  "Math.tan(Math.PI/4)",      0.9999999999999999,              Math.tan(Math.PI/4));
    array[item++] = new TestCase( SECTION,  "Math.tan(3*Math.PI/4)",    -1.0000000000000002,             Math.tan(3*Math.PI/4));
    array[item++] = new TestCase( SECTION,  "Math.tan(Math.PI)",        -1.2246063538223773e-16,             Math.tan(Math.PI));
    array[item++] = new TestCase( SECTION,  "Math.tan(5*Math.PI/4)",    0.9999999999999997,              Math.tan(5*Math.PI/4));
    array[item++] = new TestCase( SECTION,  "Math.tan(7*Math.PI/4)",    -1.0000000000000004,             Math.tan(7*Math.PI/4));
    array[item++] = new TestCase( SECTION,  "Infinity/Math.tan(-0)",    -Infinity,      Infinity/Math.tan(-0) );

/*
    Arctan (x) ~ PI/2 - 1/x   for large x.  For x = 1.6x10^16, 1/x is about the last binary digit of double precision PI/2.
    That is to say, perturbing PI/2 by this much is about the smallest rounding error possible.

    This suggests that the answer Christine is getting and a real Infinity are "adjacent" results from the tangent function.  I
    suspect that tan (PI/2 + one ulp) is a negative result about the same size as tan (PI/2) and that this pair are the closest
    results to infinity that the algorithm can deliver.

    In any case, my call is that the answer we're seeing is "right".  I suggest the test pass on any result this size or larger.
    = C =
*/
    array[item++] = new TestCase( SECTION,  "Math.tan(3*Math.PI/2) >= 5443000000000000",   true,   Math.tan(3*Math.PI/2) >= 5443000000000000 );
    array[item++] = new TestCase( SECTION,  "Math.tan(Math.PI/2) >= 5443000000000000",      true,   Math.tan(Math.PI/2) >= 5443000000000000 );

    return ( array );
}
