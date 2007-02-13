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

    var SECTION = "15.8.2.11";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Math.max(x, y)";
    var BUGNUMBER="76439";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,  "Math.max.length",              2,              Math.max.length );

    array[item++] = new TestCase( SECTION,  "Math.max()",                   -Infinity,      Math.max() );
    array[item++] = new TestCase( SECTION,  "Math.max(void 0, 1)",          Number.NaN,     Math.max( void 0, 1 ) );
    array[item++] = new TestCase( SECTION,  "Math.max(void 0, void 0)",     Number.NaN,     Math.max( void 0, void 0 ) );
    array[item++] = new TestCase( SECTION,  "Math.max(null, 1)",            1,              Math.max( null, 1 ) );
    array[item++] = new TestCase( SECTION,  "Math.max(-1, null)",           0,              Math.max( -1, null ) );
    array[item++] = new TestCase( SECTION,  "Math.max(true, false)",        1,              Math.max(true,false) );

    array[item++] = new TestCase( SECTION,  "Math.max('-99','99')",          99,             Math.max( "-99","99") );
                                                 
    array[item++] = new TestCase( SECTION,  "Math.max(NaN,Infinity)",Number.NaN,    Math.max(Number.NaN,Number.POSITIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,  "Math.max(NaN, 0)",Number.NaN,     Math.max(Number.NaN, 0) );
   
    array[item++] = new TestCase( SECTION,  "Math.max('a string', 0)",      Number.NaN,     Math.max("a string", 0) );
    array[item++] = new TestCase( SECTION,  "Math.max(NaN, 1)",             Number.NaN,     Math.max(Number.NaN,1) );
    array[item++] = new TestCase( SECTION,  "Math.max('a string',Infinity)", Number.NaN,    Math.max("a string", Number.POSITIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,  "Math.max(Infinity, NaN)",      Number.NaN,     Math.max( Number.POSITIVE_INFINITY, Number.NaN) );
    array[item++] = new TestCase( SECTION,  "Math.max(NaN, NaN)",           Number.NaN,     Math.max(Number.NaN, Number.NaN) );
    array[item++] = new TestCase( SECTION,  "Math.max(0,NaN)",              Number.NaN,     Math.max(0,Number.NaN) );
    array[item++] = new TestCase( SECTION,  "Math.max(1, NaN)",             Number.NaN,     Math.max(1, Number.NaN) );
    array[item++] = new TestCase( SECTION,  "Math.max(0,0)",                0,              Math.max(0,0) );
    array[item++] = new TestCase( SECTION,  "Math.max(0,-0)",               0,              Math.max(0,-0) );
    array[item++] = new TestCase( SECTION,  "Math.max(-0,0)",               0,              Math.max(-0,0) );
    array[item++] = new TestCase( SECTION,  "Math.max(-0,-0)",              -0,             Math.max(-0,-0) );
    array[item++] = new TestCase( SECTION,  "Infinity/Math.max(-0,-0)",              -Infinity,             Infinity/Math.max(-0,-0) );
    array[item++] = new TestCase( SECTION,  "Math.max(Infinity, Number.MAX_VALUE)", Number.POSITIVE_INFINITY,   Math.max(Number.POSITIVE_INFINITY, Number.MAX_VALUE) );
    array[item++] = new TestCase( SECTION,  "Math.max(Infinity, Infinity)",         Number.POSITIVE_INFINITY,   Math.max(Number.POSITIVE_INFINITY,Number.POSITIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,  "Math.max(-Infinity,-Infinity)",        Number.NEGATIVE_INFINITY,   Math.max(Number.NEGATIVE_INFINITY,Number.NEGATIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,  "Math.max(1,.99999999999999)",          1,                          Math.max(1,.99999999999999) );
    array[item++] = new TestCase( SECTION,  "Math.max(-1,-.99999999999999)",        -.99999999999999,           Math.max(-1,-.99999999999999) );

    array[item++] = new TestCase( SECTION,  "Math.max(Infinity, Number.MAX_VALUE,Number.MIN_VALUE,Number.NEGATIVE_INFINITY)", Number.POSITIVE_INFINITY,   Math.max(Number.POSITIVE_INFINITY,Number.MAX_VALUE,Number.MIN_VALUE,Number.NEGATIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,  "Math.max(Infinity, Number.MAX_VALUE,Number.MIN_VALUE,Number.NEGATIVE_INFINITY,0,-0,1,-1,'string',Number.NaN,null,void 0)", Number.NaN,   Math.max(Number.POSITIVE_INFINITY,Number.MAX_VALUE,Number.MIN_VALUE,Number.NEGATIVE_INFINITY,0,-0,1,-1,'string',Number.NaN,null,void 0) );
    array[item++] = new TestCase( SECTION,  "Math.max(Infinity, Number.MAX_VALUE,Number.MIN_VALUE,Number.NEGATIVE_INFINITY,0,-0,1,-1)", Infinity,   Math.max(Number.POSITIVE_INFINITY,Number.MAX_VALUE,Number.MIN_VALUE,Number.NEGATIVE_INFINITY,0,-0,1) );

    return ( array );
}

