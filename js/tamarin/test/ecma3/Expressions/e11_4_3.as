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

    var SECTION = "e11_4_3";

    var VERSION = "ECMA_1";
    startTest();
    
    var TITLE   = " The typeof operator";
    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;    

    array[item++] = new TestCase( SECTION,     "typeof(void(0))",              "undefined",        typeof(void(0)) );
    array[item++] = new TestCase( SECTION,     "typeof(null)",                 "object",           typeof(null) );
    array[item++] = new TestCase( SECTION,     "typeof(true)",                 "boolean",          typeof(true) );
    array[item++] = new TestCase( SECTION,     "typeof(false)",                "boolean",          typeof(false) );
    array[item++] = new TestCase( SECTION,     "typeof(new Boolean())",        "boolean",           typeof(new Boolean()) );
    array[item++] = new TestCase( SECTION,     "typeof(new Boolean(true))",    "boolean",           typeof(new Boolean(true)) );
    array[item++] = new TestCase( SECTION,     "typeof(Boolean())",            "boolean",          typeof(Boolean()) );
    array[item++] = new TestCase( SECTION,     "typeof(Boolean(false))",       "boolean",          typeof(Boolean(false)) );
    array[item++] = new TestCase( SECTION,     "typeof(Boolean(true))",        "boolean",          typeof(Boolean(true)) );
    array[item++] = new TestCase( SECTION,     "typeof(NaN)",                  "number",           typeof(Number.NaN) );
    array[item++] = new TestCase( SECTION,     "typeof(Infinity)",             "number",           typeof(Number.POSITIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,     "typeof(-Infinity)",            "number",           typeof(Number.NEGATIVE_INFINITY) );
    array[item++] = new TestCase( SECTION,     "typeof(Math.PI)",              "number",           typeof(Math.PI) );
    array[item++] = new TestCase( SECTION,     "typeof(0)",                    "number",           typeof(0) );
    array[item++] = new TestCase( SECTION,     "typeof(1)",                    "number",           typeof(1) );
    array[item++] = new TestCase( SECTION,     "typeof(-1)",                   "number",           typeof(-1) );
    array[item++] = new TestCase( SECTION,     "typeof('0')",                  "string",           typeof("0") );
    array[item++] = new TestCase( SECTION,     "typeof(Number())",             "number",           typeof(Number()) );
    array[item++] = new TestCase( SECTION,     "typeof(Number(0))",            "number",           typeof(Number(0)) );
    array[item++] = new TestCase( SECTION,     "typeof(Number(1))",            "number",           typeof(Number(1)) );
    array[item++] = new TestCase( SECTION,     "typeof(Nubmer(-1))",           "number",           typeof(Number(-1)) );
    array[item++] = new TestCase( SECTION,     "typeof(new Number())",         "number",           typeof(new Number()) );
    array[item++] = new TestCase( SECTION,     "typeof(new Number(0))",        "number",           typeof(new Number(0)) );
    array[item++] = new TestCase( SECTION,     "typeof(new Number(1))",        "number",           typeof(new Number(1)) );

    // Math does not implement [[Construct]] or [[Call]] so its type is object.

    array[item++] = new TestCase( SECTION,     "typeof(Math)",                 "object",         typeof(Math) );

    array[item++] = new TestCase( SECTION,     "typeof(Number.prototype.toString)", "function",    typeof(Number.prototype.toString) );

    array[item++] = new TestCase( SECTION,     "typeof('a string')",           "string",           typeof("a string") );
    array[item++] = new TestCase( SECTION,     "typeof('')",                   "string",           typeof("") );
    array[item++] = new TestCase( SECTION,     "typeof(new Date())",           "object",           typeof(new Date()) );
    array[item++] = new TestCase( SECTION,     "typeof(new Array(1,2,3))",     "object",           typeof(new Array(1,2,3)) );
    array[item++] = new TestCase( SECTION,     "typeof(new String('string object'))",  "string",   typeof(new String("string object")) );
    array[item++] = new TestCase( SECTION,     "typeof(String('string primitive'))",    "string",  typeof(String("string primitive")) );
    array[item++] = new TestCase( SECTION,     "typeof(['array', 'of', 'strings'])",   "object",   typeof(["array", "of", "strings"]) );
    array[item++] = new TestCase( SECTION,     "typeof(new Function())",                "function",     typeof( new Function() ) );
    array[item++] = new TestCase( SECTION,     "typeof(parseInt)",                      "function",     typeof( parseInt ) );
    array[item++] = new TestCase( SECTION,     "typeof(test)",                          "function",     typeof( test ) );
    array[item++] = new TestCase( SECTION,     "typeof(String.fromCharCode)",           "function",     typeof( String.fromCharCode )  );

	var notype;
	array[item++] = new TestCase( SECTION,     "typeof var notype; ",              "undefined",        typeof notype );
	var hnumber = 0x464d34;
	array[item++] = new TestCase( SECTION,     "typeof hnumber = 0x464d34; ",              "number",        typeof hnumber );
	var obj:Object;
	array[item++] = new TestCase( SECTION,     "typeof var obj:Object; ",              "object",        typeof obj );
	obj = new Object();
	array[item++] = new TestCase( SECTION,     "typeof obj = new Object()",              "object",        typeof obj );
	var dt:Date;
	array[item++] = new TestCase( SECTION,     "typeof var dt:Date; ",              "object",        typeof dt );
	dt = new Date();
	array[item++] = new TestCase( SECTION,     "typeof dt = new Date()",              "object",        typeof dt );
	var localClass:LocalClass;
	array[item++] = new TestCase( SECTION,     "typeof localClass:LocalClass; ",              "object",        typeof localClass );
	localClass = new LocalClass();
	array[item++] = new TestCase( SECTION,     "typeof localClass = new LocalClass(); ",              "object",        typeof localClass );
    array[item++] = new TestCase( SECTION,     "typeof undefined",              "undefined",        typeof undefined );
    array[item++] = new TestCase( SECTION,     "typeof void(0)",              "undefined",        typeof void(0) );
    array[item++] = new TestCase( SECTION,     "typeof null",                 "object",           typeof null );
    array[item++] = new TestCase( SECTION,     "typeof true",                 "boolean",          typeof true );
    array[item++] = new TestCase( SECTION,     "typeof false",                "boolean",          typeof false );
	array[item++] = new TestCase( SECTION,     "typeof new Boolean()",        "boolean",           typeof new Boolean() );
    array[item++] = new TestCase( SECTION,     "typeof new Boolean(true)",    "boolean",           typeof new Boolean(true) );
    array[item++] = new TestCase( SECTION,     "typeof Boolean()",            "boolean",          typeof Boolean() );
    array[item++] = new TestCase( SECTION,     "typeof Boolean(false)",       "boolean",          typeof Boolean(false) );
    array[item++] = new TestCase( SECTION,     "typeof Boolean(true)",        "boolean",          typeof Boolean(true) );
    array[item++] = new TestCase( SECTION,     "typeof Number.NaN",                  			"number",           typeof Number.NaN );
    array[item++] = new TestCase( SECTION,     "typeof Number.POSITIVE_INFINITY",             "number",           typeof Number.POSITIVE_INFINITY );
    array[item++] = new TestCase( SECTION,     "typeof Number.NEGATIVE_INFINITY",            	"number",           typeof Number.NEGATIVE_INFINITY );
    array[item++] = new TestCase( SECTION,     "typeof Math.PI",              "number",           typeof Math.PI );
    array[item++] = new TestCase( SECTION,     "typeof 0",                    "number",           typeof 0 );
    array[item++] = new TestCase( SECTION,     "typeof 1",                    "number",           typeof 1 );
    array[item++] = new TestCase( SECTION,     "typeof -1",                   "number",           typeof -1 );
    array[item++] = new TestCase( SECTION,     "typeof ''",                  "string",           typeof '' );
    array[item++] = new TestCase( SECTION,     "typeof '0'",                  "string",           typeof '0' );
    array[item++] = new TestCase( SECTION,     "typeof Number()",             "number",           typeof Number() );
    array[item++] = new TestCase( SECTION,     "typeof Number(0)",            "number",           typeof Number(0) );
    array[item++] = new TestCase( SECTION,     "typeof Number(1)",            "number",           typeof Number(1) );
    array[item++] = new TestCase( SECTION,     "typeof Nubmer(-1)",           "number",           typeof Number(-1) );
    array[item++] = new TestCase( SECTION,     "typeof new Number()",         "number",           typeof new Number() );
    array[item++] = new TestCase( SECTION,     "typeof new Number(0)",        "number",           typeof new Number(0) );
    array[item++] = new TestCase( SECTION,     "typeof new Number(1)",        "number",           typeof new Number(1) );
    array[item++] = new TestCase( SECTION,     "typeof Math",                 "object",           typeof Math );
    array[item++] = new TestCase( SECTION,     "typeof Number.prototype.toString", "function",    typeof Number.prototype.toString );
    array[item++] = new TestCase( SECTION,     "typeof String(\"a string\")",           "string",           typeof String("a string") );
    array[item++] = new TestCase( SECTION,     "typeof String(\"\")",                   "string",           typeof String("") );
    array[item++] = new TestCase( SECTION,     "typeof String(\"  \")",                   "string",           typeof String("  ") );
    array[item++] = new TestCase( SECTION,     "typeof new Date()",           "object",           typeof new Date() );
    array[item++] = new TestCase( SECTION,     "typeof new Array(1,2,3)",     "object",           typeof new Array(1,2,3) );
    array[item++] = new TestCase( SECTION,     "typeof new String('string object')",  "string",   typeof new String("string object") );
    array[item++] = new TestCase( SECTION,     "typeof String('string primitive')",    "string",  typeof String("string primitive") );
    array[item++] = new TestCase( SECTION,     "typeof ['array', 'of', 'strings']",   "object",   typeof ["array", "of", "strings"] );
    array[item++] = new TestCase( SECTION,     "typeof new Function()",                "function",     typeof new Function() );
    array[item++] = new TestCase( SECTION,     "typeof parseInt",                      "function",     typeof parseInt  );
    array[item++] = new TestCase( SECTION,     "typeof test",                          "function",     typeof test );
    array[item++] = new TestCase( SECTION,     "typeof String.fromCharCode",           "function",     typeof String.fromCharCode  );

    return array;
}

class LocalClass {
	function LocalClass() {
		trace("Constructor of LocalClass");
	}
}
