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
    var SECTION = "e11_8_3";
    var VERSION = "ECMA_1";
    startTest();
    var testcases = getTestCases();

    writeHeaderToLog( SECTION + " The less-than-or-equal operator ( <= )");
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION, "true <= false",              false,      true <= false );
    array[item++] = new TestCase( SECTION, "false <= true",              true,       false <= true );
    array[item++] = new TestCase( SECTION, "false <= false",             true,      false <= false );
    array[item++] = new TestCase( SECTION, "true <= true",               true,      true <= true );

    array[item++] = new TestCase( SECTION, "new Boolean(true) <= new Boolean(true)",     true,  new Boolean(true) <= new Boolean(true) );
    array[item++] = new TestCase( SECTION, "new Boolean(true) <= new Boolean(false)",    false,  new Boolean(true) <= new Boolean(false) );
    array[item++] = new TestCase( SECTION, "new Boolean(false) <= new Boolean(true)",    true,   new Boolean(false) <= new Boolean(true) );
    array[item++] = new TestCase( SECTION, "new Boolean(false) <= new Boolean(false)",   true,  new Boolean(false) <= new Boolean(false) );

    array[item++] = new TestCase( SECTION, "new MyObject(Infinity) <= new MyObject(Infinity)",   true,  new MyObject( Number.POSITIVE_INFINITY ) <= new MyObject( Number.POSITIVE_INFINITY) );
    array[item++] = new TestCase( SECTION, "new MyObject(-Infinity) <= new MyObject(Infinity)",  true,   new MyObject( Number.NEGATIVE_INFINITY ) <= new MyObject( Number.POSITIVE_INFINITY) );
    array[item++] = new TestCase( SECTION, "new MyObject(-Infinity) <= new MyObject(-Infinity)", true,  new MyObject( Number.NEGATIVE_INFINITY ) <= new MyObject( Number.NEGATIVE_INFINITY) );

    array[item++] = new TestCase( SECTION, "new MyValueObject(false) <= new MyValueObject(true)",  true,   new MyValueObject(false) <= new MyValueObject(true) );
    array[item++] = new TestCase( SECTION, "new MyValueObject(true) <= new MyValueObject(true)",   true,  new MyValueObject(true) <= new MyValueObject(true) );
    array[item++] = new TestCase( SECTION, "new MyValueObject(false) <= new MyValueObject(false)", true,  new MyValueObject(false) <= new MyValueObject(false) );

    array[item++] = new TestCase( SECTION, "new MyStringObject(false) <= new MyStringObject(true)",  true,   new MyStringObject(false) <= new MyStringObject(true) );
    array[item++] = new TestCase( SECTION, "new MyStringObject(true) <= new MyStringObject(true)",   true,  new MyStringObject(true) <= new MyStringObject(true) );
    array[item++] = new TestCase( SECTION, "new MyStringObject(false) <= new MyStringObject(false)", true,  new MyStringObject(false) <= new MyStringObject(false) );

    array[item++] = new TestCase( SECTION, "Number.NaN <= Number.NaN",   false,     Number.NaN <= Number.NaN );
    array[item++] = new TestCase( SECTION, "0 <= Number.NaN",            false,     0 <= Number.NaN );
    array[item++] = new TestCase( SECTION, "Number.NaN <= 0",            false,     Number.NaN <= 0 );

    array[item++] = new TestCase( SECTION, "0 <= -0",                    true,      0 <= -0 );
    array[item++] = new TestCase( SECTION, "-0 <= 0",                    true,      -0 <= 0 );

    array[item++] = new TestCase( SECTION, "Infinity <= 0",                  false,      Number.POSITIVE_INFINITY <= 0 );
    array[item++] = new TestCase( SECTION, "Infinity <= Number.MAX_VALUE",   false,      Number.POSITIVE_INFINITY <= Number.MAX_VALUE );
    array[item++] = new TestCase( SECTION, "Infinity <= Infinity",           true,       Number.POSITIVE_INFINITY <= Number.POSITIVE_INFINITY );

    array[item++] = new TestCase( SECTION, "0 <= Infinity",                  true,       0 <= Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION, "Number.MAX_VALUE <= Infinity",   true,       Number.MAX_VALUE <= Number.POSITIVE_INFINITY );

    array[item++] = new TestCase( SECTION, "0 <= -Infinity",                 false,      0 <= Number.NEGATIVE_INFINITY );
    array[item++] = new TestCase( SECTION, "Number.MAX_VALUE <= -Infinity",  false,      Number.MAX_VALUE <= Number.NEGATIVE_INFINITY );
    array[item++] = new TestCase( SECTION, "-Infinity <= -Infinity",         true,       Number.NEGATIVE_INFINITY <= Number.NEGATIVE_INFINITY );

    array[item++] = new TestCase( SECTION, "-Infinity <= 0",                 true,       Number.NEGATIVE_INFINITY <= 0 );
    array[item++] = new TestCase( SECTION, "-Infinity <= -Number.MAX_VALUE", true,       Number.NEGATIVE_INFINITY <= -Number.MAX_VALUE );
    array[item++] = new TestCase( SECTION, "-Infinity <= Number.MIN_VALUE",  true,       Number.NEGATIVE_INFINITY <= Number.MIN_VALUE );

    array[item++] = new TestCase( SECTION, "'string' <= 'string'",           true,       'string' <= 'string' );
    array[item++] = new TestCase( SECTION, "'astring' <= 'string'",          true,       'astring' <= 'string' );
    array[item++] = new TestCase( SECTION, "'strings' <= 'stringy'",         true,       'strings' <= 'stringy' );
    array[item++] = new TestCase( SECTION, "'strings' <= 'stringier'",       false,       'strings' <= 'stringier' );
    array[item++] = new TestCase( SECTION, "'string' <= 'astring'",          false,      'string' <= 'astring' );
    array[item++] = new TestCase( SECTION, "'string' <= 'strings'",          true,       'string' <= 'strings' );
    array[item++] = new TestCase( SECTION, "'string' <= 'str'",              false,       'string' <= 'str' );
	
    array[item++] = new TestCase( SECTION, "'string' <= undefined",          false,       'string' <= undefined );
    array[item++] = new TestCase( SECTION, "undefined  <= 'string'",          false,       undefined  <= 'string' );
    array[item++] = new TestCase( SECTION, "6.9 <= undefined",          	 false,       6.9 <= undefined );
    array[item++] = new TestCase( SECTION, "undefined <= 343",          	 false,       undefined <= 343);

    array[item++] = new TestCase( SECTION, "44 <= 55",          true,       44 <= 55 );
    array[item++] = new TestCase( SECTION, "55 <= 44",          false,       55 <= 44 );
    array[item++] = new TestCase( SECTION, "56.43 <= 65.0",          true,       56.43 <= 65.0 );
    array[item++] = new TestCase( SECTION, "65.0 <= 56.43",          false,       65.0 <= 56.43 );
    array[item++] = new TestCase( SECTION, "43247503.43 <= 945540654.654",          true,       43247503.43 <= 945540654.654 );
    array[item++] = new TestCase( SECTION, "43247503.43<=945540654.654",          true,       43247503.43<=945540654.654 );
    array[item++] = new TestCase( SECTION, "43247503.43<= 945540654.654",          true,       43247503.43<= 945540654.654 );
    array[item++] = new TestCase( SECTION, "43247503.43   <=  945540654.654",          true,       43247503.43   <=  945540654.654 );
    array[item++] = new TestCase( SECTION, "-56.43 <= 65.0",          true,       -56.43 <= 65.0 );
    array[item++] = new TestCase( SECTION, "-56.43 <= -65.0",          false,       -56.43 <= -65.0 );
    array[item++] = new TestCase( SECTION, "100 <= 100",               true,      100 <= 100 );
    array[item++] = new TestCase( SECTION, "-56.43 <= -56.43",          true,       -56.43 <= -56.43 );

    return ( array );
}
function MyObject(value) {
    this.value = value;
    //this.valueOf = new Function( "return this.value" );
    //this.toString = new Function( "return this.value +''" );
    this.valueOf = function(){return this.value};
    this.toString = function(){return this.value+''};
}
function MyValueObject(value) {
    this.value = value;
//  this.valueOf = new Function( "return this.value" );
    this.valueOf = function(){return this.value};
}
function MyStringObject(value) {
    this.value = value;
//  this.toString = new Function( "return this.value +''" );
    this.toString = function(){return this.value+''};
}
