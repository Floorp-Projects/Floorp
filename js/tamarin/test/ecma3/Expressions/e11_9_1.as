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
    var SECTION = "11.9.1";
    var VERSION = "ECMA_1";
    startTest();
    var BUGNUMBER="77391";


    var testcases = getTestCases();

    writeHeaderToLog( SECTION + " The equals operator ( == )");
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    // type x and type y are the same.  if type x is undefined or null, return true

    array[item++] = new TestCase( SECTION,    "void 0 = void 0",        true,   void 0 == void 0 );
    array[item++] = new TestCase( SECTION,    "null == null",           true,   null == null );

    //  if x is NaN, return false. if y is NaN, return false.

    array[item++] = new TestCase( SECTION,    "NaN == NaN",             false,  Number.NaN == Number.NaN );
    array[item++] = new TestCase( SECTION,    "NaN == 0",               false,  Number.NaN == 0 );
    array[item++] = new TestCase( SECTION,    "0 == NaN",               false,  0 == Number.NaN );
    array[item++] = new TestCase( SECTION,    "NaN == Infinity",        false,  Number.NaN == Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "Infinity == NaN",        false,  Number.POSITIVE_INFINITY == Number.NaN );

    // if x is the same number value as y, return true.

    array[item++] = new TestCase( SECTION,    "Number.MAX_VALUE == Number.MAX_VALUE",   true,   Number.MAX_VALUE == Number.MAX_VALUE );
    array[item++] = new TestCase( SECTION,    "Number.MIN_VALUE == Number.MIN_VALUE",   true,   Number.MIN_VALUE == Number.MIN_VALUE );
    array[item++] = new TestCase( SECTION,    "Number.POSITIVE_INFINITY == Number.POSITIVE_INFINITY",   true,   Number.POSITIVE_INFINITY == Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION,    "Number.NEGATIVE_INFINITY == Number.NEGATIVE_INFINITY",   true,   Number.NEGATIVE_INFINITY == Number.NEGATIVE_INFINITY );

    //  if xis 0 and y is -0, return true.   if x is -0 and y is 0, return true.

    array[item++] = new TestCase( SECTION,    "0 == 0",                 true,   0 == 0 );
    array[item++] = new TestCase( SECTION,    "0 == -0",                true,   0 == -0 );
    array[item++] = new TestCase( SECTION,    "-0 == 0",                true,   -0 == 0 );
    array[item++] = new TestCase( SECTION,    "-0 == -0",               true,   -0 == -0 );

    // return false.

    array[item++] = new TestCase( SECTION,    "0.9 == 1",               false,  0.9 == 1 );
    array[item++] = new TestCase( SECTION,    "0.999999 == 1",          false,  0.999999 == 1 );
    array[item++] = new TestCase( SECTION,    "0.9999999999 == 1",      false,  0.9999999999 == 1 );
    array[item++] = new TestCase( SECTION,    "0.9999999999999 == 1",   false,  0.9999999999999 == 1 );

    // type x and type y are the same type, but not numbers.


    // x and y are strings.  return true if x and y are exactly the same sequence of characters.
    // otherwise, return false.

    array[item++] = new TestCase( SECTION,    "'hello' == 'hello'",         true,   "hello" == "hello" );

    // x and y are booleans.  return true if both are true or both are false.

    array[item++] = new TestCase( SECTION,    "true == true",               true,   true == true );
    array[item++] = new TestCase( SECTION,    "false == false",             true,   false == false );
    array[item++] = new TestCase( SECTION,    "true == false",              false,  true == false );
    array[item++] = new TestCase( SECTION,    "false == true",              false,  false == true );

    // return true if x and y refer to the same object.  otherwise return false.

    array[item++] = new TestCase( SECTION,    "new MyObject(true) == new MyObject(true)",   false,  new MyObject(true) == new MyObject(true) );
    array[item++] = new TestCase( SECTION,    "new Boolean(true) == new Boolean(true)",     true,  new Boolean(true) == new Boolean(true) );
    array[item++] = new TestCase( SECTION,    "new Boolean(false) == new Boolean(false)",   true,  new Boolean(false) == new Boolean(false) );

    x = new MyObject(true); y = x; z = x;
    array[item++] = new TestCase( SECTION,    "x = new MyObject(true); y = x; z = x; z == y",   true,   z == y );
    x = new MyObject(false); y = x; z = x; 
    array[item++] = new TestCase( SECTION,    "x = new MyObject(false); y = x; z = x; z == y",  true,  z == y );
    x = new Boolean(true); y = x; z = x; 
    array[item++] = new TestCase( SECTION,    "x = new Boolean(true); y = x; z = x; z == y",   true,  z == y );
    x = new Boolean(false); y = x; z = x;
    array[item++] = new TestCase( SECTION,   "x = new Boolean(false); y = x; z = x; z == y",   true,  z == y );

    array[item++] = new TestCase( SECTION,    "new Boolean(true) == new Boolean(true)",     true,  new Boolean(true) == new Boolean(true) );
    array[item++] = new TestCase( SECTION,    "new Boolean(false) == new Boolean(false)",   true,  new Boolean(false) == new Boolean(false) );

    // if x is null and y is undefined, return true.  if x is undefined and y is null return true.

    array[item++] = new TestCase( SECTION,    "null == void 0",             true,   null == void 0 );
    array[item++] = new TestCase( SECTION,    "void 0 == null",             true,   void 0 == null );

    // if type(x) is Number and type(y) is string, return the result of the comparison x == ToNumber(y).

    array[item++] = new TestCase( SECTION,    "1 == '1'",                   true,   1 == '1' );
    array[item++] = new TestCase( SECTION,    "255 == '0xff'",               true,  255 == '0xff' );
    array[item++] = new TestCase( SECTION,    "0 == '\r'",                  true,   0 == "\r" );
    array[item++] = new TestCase( SECTION,    "1e19 == '1e19'",             true,   1e19 == "1e19" );


    array[item++] = new TestCase( SECTION,    "new Boolean(true) == true",  true,   true == new Boolean(true) );
    array[item++] = new TestCase( SECTION,    "new MyObject(true) == true", true,   true == new MyObject(true) );

    array[item++] = new TestCase( SECTION,    "new Boolean(false) == false",    true,   new Boolean(false) == false );
    array[item++] = new TestCase( SECTION,    "new MyObject(false) == false",   true,   new MyObject(false) == false );

    array[item++] = new TestCase( SECTION,    "true == new Boolean(true)",      true,   true == new Boolean(true) );
    array[item++] = new TestCase( SECTION,    "true == new MyObject(true)",     true,   true == new MyObject(true) );

    array[item++] = new TestCase( SECTION,    "false == new Boolean(false)",    true,   false == new Boolean(false) );
    array[item++] = new TestCase( SECTION,    "false == new MyObject(false)",   true,   false == new MyObject(false) );

    return ( array );
}

function MyObject( value ) {
    this.value = value;
    //this.valueOf = new Function( "return this.value" );
    this.valueOf = function(){return this.value};
}
