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

    var VERSION = "ECMA_1";
    startTest();
    var SECTION = "9.9-1";

    writeHeaderToLog( SECTION + " Type Conversion: ToObject" );
    var tc= 0;
    var testcases = getTestCases();

//  all tests must call a function that returns an array of TestCase objects.
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION, "Object(true).valueOf()",    true,                   (Object(true)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(true)",       "boolean",               typeof Object(true) );
    array[item++] = new TestCase( SECTION, "(Object(true)).__proto__",  Boolean.prototype,      (Object(true)).constructor.prototype);

    array[item++] = new TestCase( SECTION, "Object(false).valueOf()",    false,                  (Object(false)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(false)",      "boolean",               typeof Object(false) );
    array[item++] = new TestCase( SECTION, "(Object(true)).__proto__",  Boolean.prototype,      (Object(true)).constructor.prototype);

    array[item++] = new TestCase( SECTION, "Object(0).valueOf()",       0,                      (Object(0)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(0)",          "number",               typeof Object(0) );
    array[item++] = new TestCase( SECTION, "(Object(0)).__proto__",     Number.prototype,      (Object(0)).constructor.prototype);

    array[item++] = new TestCase( SECTION, "Object(-0).valueOf()",      -0,                     (Object(-0)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(-0)",         "number",               typeof Object(-0) );
    array[item++] = new TestCase( SECTION, "(Object(-0)).__proto__",    Number.prototype,      (Object(-0)).constructor.prototype);

    array[item++] = new TestCase( SECTION, "Object(1).valueOf()",       1,                      (Object(1)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(1)",          "number",               typeof Object(1) );
    array[item++] = new TestCase( SECTION, "(Object(1)).__proto__",     Number.prototype,      (Object(1)).constructor.prototype);

    array[item++] = new TestCase( SECTION, "Object(-1).valueOf()",      -1,                     (Object(-1)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(-1)",         "number",               typeof Object(-1) );
    array[item++] = new TestCase( SECTION, "(Object(-1)).__proto__",    Number.prototype,      (Object(-1)).constructor.prototype);

    array[item++] = new TestCase( SECTION, "Object(Number.MAX_VALUE).valueOf()",    1.7976931348623157e308,         (Object(Number.MAX_VALUE)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(Number.MAX_VALUE)",       "number",                       typeof Object(Number.MAX_VALUE) );
    array[item++] = new TestCase( SECTION, "(Object(Number.MAX_VALUE)).__proto__",  Number.prototype,               (Object(Number.MAX_VALUE)).constructor.prototype);

    array[item++] = new TestCase( SECTION, "Object(Number.MIN_VALUE).valueOf()",     5e-324,           (Object(Number.MIN_VALUE)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(Number.MIN_VALUE)",       "number",         typeof Object(Number.MIN_VALUE) );
    array[item++] = new TestCase( SECTION, "(Object(Number.MIN_VALUE)).__proto__",  Number.prototype, (Object(Number.MIN_VALUE)).constructor.prototype);

    array[item++] = new TestCase( SECTION, "Object(Number.POSITIVE_INFINITY).valueOf()",    Number.POSITIVE_INFINITY,       (Object(Number.POSITIVE_INFINITY)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(Number.POSITIVE_INFINITY)",       "number",                       typeof Object(Number.POSITIVE_INFINITY) );
    array[item++] = new TestCase( SECTION, "(Object(Number.POSITIVE_INFINITY)).__proto__",  Number.prototype,               (Object(Number.POSITIVE_INFINITY)).constructor.prototype);

    array[item++] = new TestCase( SECTION, "Object(Number.NEGATIVE_INFINITY).valueOf()",    Number.NEGATIVE_INFINITY,       (Object(Number.NEGATIVE_INFINITY)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(Number.NEGATIVE_INFINITY)",       "number",            typeof Object(Number.NEGATIVE_INFINITY) );
    array[item++] = new TestCase( SECTION, "(Object(Number.NEGATIVE_INFINITY)).__proto__",  Number.prototype,   (Object(Number.NEGATIVE_INFINITY)).constructor.prototype);

    array[item++] = new TestCase( SECTION, "Object(Number.NaN).valueOf()",      Number.NaN,                (Object(Number.NaN)).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object(Number.NaN)",         "number",                  typeof Object(Number.NaN) );
    array[item++] = new TestCase( SECTION, "(Object(Number.NaN)).__proto__",    Number.prototype,          (Object(Number.NaN)).constructor.prototype);

    array[item++] = new TestCase( SECTION, "Object('a string').valueOf()",      "a string",         (Object("a string")).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object('a string')",         "string",           typeof (Object("a string")) );
    array[item++] = new TestCase( SECTION, "(Object('a string')).__proto__",    String.prototype,   (Object("a string")).constructor.prototype);

    array[item++] = new TestCase( SECTION, "Object('').valueOf()",              "",                 (Object("")).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object('')",                 "string",           typeof (Object("")) );
    array[item++] = new TestCase( SECTION, "(Object('')).__proto__",            String.prototype,   (Object("")).constructor.prototype);

    array[item++] = new TestCase( SECTION, "Object('\\r\\t\\b\\n\\v\\f').valueOf()",   "\r\t\b\n\v\f",   (Object("\r\t\b\n\v\f")).valueOf() );
    array[item++] = new TestCase( SECTION, "typeof Object('\\r\\t\\b\\n\\v\\f')",      "string",           typeof (Object("\\r\\t\\b\\n\\v\\f")) );
    array[item++] = new TestCase( SECTION, "(Object('\\r\\t\\b\\n\\v\\f')).__proto__", String.prototype,   (Object("\\r\\t\\b\\n\\v\\f")).constructor.prototype);

    array[item++] = new TestCase( SECTION,  "Object( '\\\'\\\"\\' ).valueOf()",      "\'\"\\",          (Object("\'\"\\")).valueOf() );
    array[item++] = new TestCase( SECTION,  "typeof Object( '\\\'\\\"\\' )",        "string",           typeof Object("\'\"\\") );
    array[item++] = new TestCase( SECTION,  "Object( '\\\'\\\"\\' ).__proto__",      String.prototype,   (Object("\'\"\\")).constructor.prototype);

    array[item++] = new TestCase( SECTION, "Object( new MyObject(true) ).valueOf()",    true,           Object( new MyObject(true) ).valueOf());
    array[item++] = new TestCase( SECTION, "typeof Object( new MyObject(true) )",       "object",       typeof Object( new MyObject(true))  );
    array[item++] = new TestCase( SECTION, "(Object( new MyObject(true) )).toString()",  "[object Object]",       Object( new MyObject(true) ).toString());

    return ( array );
}

function MyObject( value ) {
    this.value = value;
   // this.valueOf = new Function ( "return this.value" );
    this.valueOf = function (){ return this.value; }
}
