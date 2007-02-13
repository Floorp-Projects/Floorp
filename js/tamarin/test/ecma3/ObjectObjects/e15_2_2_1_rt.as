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

    var SECTION = "15.2.2.1";
    var VERSION = "ECMA_4";
    startTest();
    var TITLE   = "new Object( value )";

    writeHeaderToLog( SECTION + " "+ TITLE);

    var testcases = getTestCases();
    test();

function getTestCases() {
    var array = new Array();
    var item = 0;

    array[item++] = new TestCase( SECTION,  "typeof new Object(null)",      "object",           typeof new Object(null) );
    MYOB = new Object(null);
    MYOB.toString = Object.prototype.toString;
    array[item++] = new TestCase( SECTION,  "MYOB = new Object(null); MYOB.toString = Object.prototype.toString; MYOB.toString()",  "[object Object]",    MYOB.toString());

    array[item++] = new TestCase( SECTION,  "typeof new Object(void 0)",      "object",           typeof new Object(void 0) );
	MYOB = new Object(new Object(void 0));
	MYOB.toString = Object.prototype.toString;
    array[item++] = new TestCase( SECTION,  "MYOB = new Object(new Object(void 0)); MYOB.toString = Object.prototype.toString; MYOB.toString()",  "[object Object]",    MYOB.toString());

    array[item++] = new TestCase( SECTION,  "typeof new Object(undefined)",      "object",           typeof new Object(undefined) );
	MYOB = new Object(new Object(undefined));
	MYOB.toString = Object.prototype.toString;
    array[item++] = new TestCase( SECTION,  "MYOB = new Object(new Object(undefined)); MYOB.toString = Object.prototype.toString; MYOB.toString()",  "[object Object]",    MYOB.toString());

    array[item++] = new TestCase( SECTION,  "typeof new Object('string')",      "string",           typeof new Object('string') );

    MYOB = new Object('string');
    array[item++] = new TestCase( SECTION,  "MYOB = (new Object('string'); MYOB.toString = Object.prototype.toString; MYOB.toString()",  "string", MYOB.toString());

    array[item++] = new TestCase( SECTION,  "(new Object('string').valueOf()",  "string",           (new Object('string')).valueOf() );

    array[item++] = new TestCase( SECTION,  "typeof new Object('')",            "string",           typeof new Object('') );
    MYOB = new Object('');
    array[item++] = new TestCase( SECTION,  "MYOB = (new Object(''); MYOB.toString = Object.prototype.toString; MYOB.toString()",  "", MYOB.toString());

    array[item++] = new TestCase( SECTION,  "(new Object('').valueOf()",        "",                 (new Object('')).valueOf() );

    array[item++] = new TestCase( SECTION,  "typeof new Object(Number.NaN)",      "number",                 typeof new Object(Number.NaN) );
    MYOB = new Object(Number.NaN);
    array[item++] = new TestCase( SECTION,  "MYOB = (new Object(Number.NaN); MYOB.toStriobjectng = Object.prototype.toString; MYOB.toString()",  "NaN", MYOB.toString() );
    array[item++] = new TestCase( SECTION,  "(new Object(Number.NaN).valueOf()",  Number.NaN,               (new Object(Number.NaN)).valueOf() );

    array[item++] = new TestCase( SECTION,  "typeof new Object(0)",      "number",                 typeof new Object(0) );
    MYOB = new Object(0);
    array[item++] = new TestCase( SECTION,  "MYOB = (new Object(0); MYOB.toString = Object.prototype.toString; MYOB.toString()",  "0", MYOB.toString());
    array[item++] = new TestCase( SECTION,  "(new Object(0).valueOf()",  0,               (new Object(0)).valueOf() );

    array[item++] = new TestCase( SECTION,  "typeof new Object(-0)",      "number",                 typeof new Object(-0) );
    MYOB = new Object(-0);
    array[item++] = new TestCase( SECTION,  "MYOB = (new Object(-0); MYOB.toString = Object.prototype.toString; MYOB.toString()",  "0", MYOB.toString() );
    array[item++] = new TestCase( SECTION,  "(new Object(-0).valueOf()",  -0,               (new Object(-0)).valueOf() );

    array[item++] = new TestCase( SECTION,  "typeof new Object(1)",      "number",                 typeof new Object(1) );
    MYOB = new Object(1);
    array[item++] = new TestCase( SECTION,  "MYOB = (new Object(1); MYOB.toString = Object.prototype.toString; MYOB.toString()",  "1",    MYOB.toString() );
    array[item++] = new TestCase( SECTION,  "(new Object(1).valueOf()",  1,               (new Object(1)).valueOf() );

    array[item++] = new TestCase( SECTION,  "typeof new Object(-1)",      "number",                 typeof new Object(-1) );
    MYOB = new Object(-1);
    array[item++] = new TestCase( SECTION,  "MYOB = (new Object(-1); MYOB.toString = Object.prototype.toString; MYOB.toString()",  "-1",    MYOB.toString() );
    array[item++] = new TestCase( SECTION,  "(new Object(-1).valueOf()",  -1,               (new Object(-1)).valueOf() );

    array[item++] = new TestCase( SECTION,  "typeof new Object(true)",      "boolean",                 typeof new Object(true) );
	Boolean.prototype.valueOf=Object.prototype.valueOf;
	MYOB = new Object(true);
    array[item++] = new TestCase( SECTION,  "MYOB = (new Object(true); MYOB.toString = Object.prototype.toString; MYOB.toString()",  "true",  MYOB.toString() );
    array[item++] = new TestCase( SECTION,  "(new Object(true).valueOf()",  true,               (new Object(true)).valueOf() );

    array[item++] = new TestCase( SECTION,  "typeof new Object(false)",      "boolean",              typeof new Object(false) );
    MYOB = new Object(false);
    array[item++] = new TestCase( SECTION,  "MYOB = (new Object(false); MYOB.toString = Object.prototype.toString; MYOB.toString()",  "false",  MYOB.toString() );
    array[item++] = new TestCase( SECTION,  "(new Object(false).valueOf()",  false,                 (new Object(false)).valueOf() );

    array[item++] = new TestCase( SECTION,  "typeof new Object(Boolean())",         "boolean",               typeof new Object(Boolean()) );
    MYOB = new Object(Boolean());
    array[item++] = new TestCase( SECTION,  "MYOB = (new Object(Boolean()); MYOB.toString = Object.prototype.toString; MYOB.toString()",  "false",  MYOB.toString() );
    array[item++] = new TestCase( SECTION,  "(new Object(Boolean()).valueOf()",     Boolean(),              (new Object(Boolean())).valueOf() );

    var myglobal    = this;
    var myobject    = new Object( "my new object" );
    var myarray     = new Array();
    var myboolean   = new Boolean();
    var mynumber    = new Number();
    var mystring    = new String();
    var myobject    = new Object();
    myfunction  	= function(x){return x;}
    var mymath      = Math;
    var myregexp    = new RegExp(new String(''));

    function myobj():String{
        function f():String{
            return "hi!";
        }
    return f();
    }

    array[item++] = new TestCase( SECTION, "myglobal = new Object( this )",                     myglobal,       new Object(this) );
    array[item++] = new TestCase( SECTION, "myobject = new Object('my new object'); new Object(myobject)",            myobject,       new Object(myobject) );
    array[item++] = new TestCase( SECTION, "myarray = new Array(); new Object(myarray)",        myarray,        new Object(myarray) );
    array[item++] = new TestCase( SECTION, "myboolean = new Boolean(); new Object(myboolean)",  myboolean,      new Object(myboolean) );
    array[item++] = new TestCase( SECTION, "mynumber = new Number(); new Object(mynumber)",     mynumber,       new Object(mynumber) );
    array[item++] = new TestCase( SECTION, "mystring = new String(); new Object(mystring)",     mystring,       new Object(mystring) );
    array[item++] = new TestCase( SECTION, "myobject = new Object(); new Object(myobject)",    myobject,       new Object(myobject) );
    array[item++] = new TestCase( SECTION, "myfunction = function(x){return x;} new Object(myfunction)", myfunction,   new Object(myfunction) );
    array[item++] = new TestCase( SECTION, "function myobj(){function f(){}return f() new Object(myobj)", myobj,   new Object(myobj) );
    array[item++] = new TestCase( SECTION, "mymath = Math; new Object(mymath)",                 mymath,         new Object(mymath) );
    array[item++] = new TestCase( SECTION, "myregexp = new RegExp(new String('')), new Object(myregexp)",                 myregexp,         new Object(myregexp) );

    return ( array );
}
