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
    var SECTION = "11_2_1_1";
    var VERSION = "ECMA_1";
    startTest();
    var TITLE   = "Property Accessors";
    writeHeaderToLog( SECTION + " "+TITLE );
    var testcases = getTestCases();
    
    test();
    
function getTestCases() {
    var array = new Array();
    var item = 0;
        
    var PROPERTY = new Array();


    array[item++] = new TestCase( SECTION,  "typeof this.NaN", "undefined", typeof (this.NaN) );
    array[item++] = new TestCase( SECTION,  "typeof this['NaN']", "undefined", typeof this['NaN'] );
    array[item++] = new TestCase( SECTION,  "typeof this.Infinity",     "undefined", typeof this.Infinity );
    array[item++] = new TestCase( SECTION,  "typeof this['Infinity']",  "undefined", typeof this['Infinity'] );
    array[item++] = new TestCase( SECTION,  "typeof this.parseInt",     "undefined", typeof this.parseInt );
    array[item++] = new TestCase( SECTION,  "typeof this['parseInt']",  "undefined", typeof this['parseInt']);
    array[item++] = new TestCase( SECTION,  "typeof this.parseFloat",   "undefined",typeof this.parseFloat );
    array[item++] = new TestCase( SECTION,  "typeof this['parseFloat']","undefined",typeof this['parseFloat']);
    array[item++] = new TestCase( SECTION,  "typeof this.escape",       "undefined",typeof this.escape );
    array[item++] = new TestCase( SECTION,  "typeof this['escape']",    "undefined",typeof this['escape']);
    array[item++] = new TestCase( SECTION,  "typeof this.unescape",     "undefined",typeof this.unescape );
    array[item++] = new TestCase( SECTION,  "typeof this['unescape']",  "undefined",typeof this['unescape']);
    array[item++] = new TestCase( SECTION,  "typeof this.isNaN",        "undefined",typeof this.isNaN );
    array[item++] = new TestCase( SECTION,  "typeof this['isNaN']",     "undefined",typeof this['isNaN']);
    array[item++] = new TestCase( SECTION,  "typeof this.isFinite",     "undefined",typeof this.isFinite );
    array[item++] = new TestCase( SECTION,  "typeof this['isFinite']",  "undefined",typeof this['isFinite']);
    array[item++] = new TestCase( SECTION,  "typeof this.Object",       "undefined", typeof this.Object);
    array[item++] = new TestCase( SECTION,  "typeof this['Object']",    "undefined", typeof this['Object']);
    array[item++] = new TestCase( SECTION,  "typeof this.Number",       "undefined", typeof this.Number);
    array[item++] = new TestCase( SECTION,  "typeof this['Number']",    "undefined", typeof this['Number']);
    array[item++] = new TestCase( SECTION,  "typeof this.Function",     "undefined", typeof this.Function);
    array[item++] = new TestCase( SECTION,  "typeof this['Function']",  "undefined", typeof this['Function']);
    array[item++] = new TestCase( SECTION,  "typeof this.Array",        "undefined", typeof this.Array);
    array[item++] = new TestCase( SECTION,  "typeof this['Array']",     "undefined", typeof this['Array']);
    array[item++] = new TestCase( SECTION,  "typeof this.String",       "undefined", typeof this.String);
    array[item++] = new TestCase( SECTION,  "typeof this['String']",    "undefined", typeof this['String']);
    array[item++] = new TestCase( SECTION,  "typeof this.Boolean",      "undefined", typeof this.Boolean);
    array[item++] = new TestCase( SECTION,  "typeof this['Boolean']",   "undefined", typeof this['Boolean']);
    array[item++] = new TestCase( SECTION,  "typeof this.Date",         "undefined", typeof this.Date);
    array[item++] = new TestCase( SECTION,  "typeof this['Date']",      "undefined", typeof this['Date']);
    array[item++] = new TestCase( SECTION,  "typeof this.Math",         "undefined", typeof this.Math);
    array[item++] = new TestCase( SECTION,  "typeof this['Math']",      "undefined", typeof this['Math']);

    // properties and  methods of Object objects

    array[item++] = new TestCase( SECTION,  "typeof Object.prototype", "object", typeof Object.prototype );
    array[item++] = new TestCase( SECTION,  "typeof Object['prototype']",    "object",typeof Object['prototype']);
    array[item++] = new TestCase( SECTION,  "typeof Object.toString",     "function", typeof Object.toString);
    array[item++] = new TestCase( SECTION,  "typeof Object['toString']",     "function", typeof Object['toString']);
    array[item++] = new TestCase( SECTION,  "typeof Object.valueOf",      "function", typeof Object.valueOf);
    array[item++] = new TestCase( SECTION,  "typeof Object['valueOf']",      "function", typeof Object['valueOf']);
    array[item++] = new TestCase( SECTION,  "typeof Object.constructor",  "object",typeof Object.constructor );
    array[item++] = new TestCase( SECTION,  "typeof Object['constructor']",  "object",typeof Object['constructor']);


    // properties of the Function object

    array[item++] = new TestCase( SECTION,  "typeof Function.prototype",              "function",typeof Function.prototype );
    array[item++] = new TestCase( SECTION,  "typeof Function['prototype']",		   "function",typeof Function['prototype'] );
    array[item++] = new TestCase( SECTION,  "typeof Function.prototype.toString",     "function",typeof Function.prototype.toString );
    array[item++] = new TestCase( SECTION,  "typeof Function.prototype['toString']",     "function",typeof Function.prototype['toString'] );
    array[item++] = new TestCase( SECTION,  "typeof Function.prototype.length",       "number", typeof Function.prototype.length);
    array[item++] = new TestCase( SECTION,  "typeof Function.prototype['length']",       "number", typeof Function.prototype['length']);
    array[item++] = new TestCase( SECTION,  "typeof Function.prototype.valueOf",      "function",typeof Function.prototype.valueOf );
    array[item++] = new TestCase( SECTION,  "typeof Function.prototype['valueOf']",      "function",typeof Function.prototype['valueOf'] );

    //created extra property.  need to delete this at the end
    Function.prototype.myProperty = "hi";

    array[item++] = new TestCase( SECTION,  "typeof Function.prototype.myProperty",   "string",typeof Function.prototype.myProperty );
    array[item++] = new TestCase( SECTION,  "typeof Function.prototype['myProperty']",   "string",typeof Function.prototype['myProperty']);

    // properties of the Array object
    array[item++] = new TestCase( SECTION,  "typeof Array.prototype",    "object",typeof Array.prototype );
    array[item++] = new TestCase( SECTION,  "typeof Array['prototype']",    "object",typeof Array['prototype']);
    array[item++] = new TestCase( SECTION,  "typeof Array.length",       "number",typeof Array.length );
    array[item++] = new TestCase( SECTION,  "typeof Array['length']",       "number",typeof Array['length']);
    array[item++] = new TestCase( SECTION,  "typeof Array.prototype.constructor",  "object",typeof Array.prototype.constructor );
    array[item++] = new TestCase( SECTION,  "typeof Array.prototype['constructor']",  "object",typeof Array.prototype['constructor']);
    array[item++] = new TestCase( SECTION,  "typeof Array.prototype.toString",     "function",typeof Array.prototype.toString );
    array[item++] = new TestCase( SECTION,  "typeof Array.prototype['toString']",     "function",typeof Array.prototype['toString']);
    array[item++] = new TestCase( SECTION,  "typeof Array.prototype.join",         "function",typeof Array.prototype.join );
    array[item++] = new TestCase( SECTION,  "typeof Array.prototype['join']",         "function",typeof Array.prototype['join']);
    array[item++] = new TestCase( SECTION,  "typeof Array.prototype.reverse",      "function",typeof Array.prototype.reverse );
    array[item++] = new TestCase( SECTION,  "typeof Array.prototype['reverse']",      "function",typeof Array.prototype['reverse']);
    array[item++] = new TestCase( SECTION,  "typeof Array.prototype.sort",         "function",typeof Array.prototype.sort );
    array[item++] = new TestCase( SECTION,  "typeof Array.prototype['sort']",         "function",typeof Array.prototype['sort']);


    // properties of the String object
    array[item++] = new TestCase( SECTION,  "typeof String.prototype",    "object",typeof String.prototype );
    array[item++] = new TestCase( SECTION,  "typeof String['prototype']",    "object",typeof String['prototype'] );
    array[item++] = new TestCase( SECTION,  "typeof String.fromCharCode", "function",typeof String.fromCharCode );
    array[item++] = new TestCase( SECTION,  "typeof String['fromCharCode']", "function",typeof String['fromCharCode'] );
    array[item++] = new TestCase( SECTION,  "typeof String.prototype.toString",     "function",typeof String.prototype.toString );
    array[item++] = new TestCase( SECTION,  "typeof String.prototype['toString']",     "function",typeof String.prototype['toString'] );
    array[item++] = new TestCase( SECTION,  "typeof String.prototype.constructor",  "object",typeof String.prototype.constructor );
    array[item++] = new TestCase( SECTION,  "typeof String.prototype['constructor']",  "object",typeof String.prototype['constructor'] );
    array[item++] = new TestCase( SECTION,  "typeof String.prototype.valueOf",      "function",typeof String.prototype.valueOf );
    array[item++] = new TestCase( SECTION,  "typeof String.prototype['valueOf']",      "function",typeof String.prototype['valueOf'] );
    array[item++] = new TestCase( SECTION,  "typeof String.prototype.charAt",       "function", typeof String.prototype.charAt);
    array[item++] = new TestCase( SECTION,  "typeof String.prototype['charAt']",       "function", typeof String.prototype['charAt']);
    array[item++] = new TestCase( SECTION,  "typeof String.prototype.charCodeAt",   "function", typeof String.prototype.charCodeAt);
    array[item++] = new TestCase( SECTION,  "typeof String.prototype['charCodeAt']",   "function", typeof String.prototype['charCodeAt']);
    array[item++] = new TestCase( SECTION,  "typeof String.prototype.indexOf",      "function",typeof String.prototype.indexOf );
    array[item++] = new TestCase( SECTION,  "typeof String.prototype['indexOf']",      "function",typeof String.prototype['indexOf'] );
    array[item++] = new TestCase( SECTION,  "typeof String.prototype.lastIndexOf",  "function",typeof String.prototype.lastIndexOf );
    array[item++] = new TestCase( SECTION,  "typeof String.prototype['lastIndexOf']",  "function",typeof String.prototype['lastIndexOf'] );
    array[item++] = new TestCase( SECTION,  "typeof String.prototype.split",        "function", typeof String.prototype.split);
    array[item++] = new TestCase( SECTION,  "typeof String.prototype['split']",        "function", typeof String.prototype['split']);
    array[item++] = new TestCase( SECTION,  "typeof String.prototype.substring",    "function",typeof String.prototype.substring );
    array[item++] = new TestCase( SECTION,  "typeof String.prototype['substring']",    "function",typeof String.prototype['substring'] );
    array[item++] = new TestCase( SECTION,  "typeof String.prototype.toLowerCase",  "function", typeof String.prototype.toLowerCase);
    array[item++] = new TestCase( SECTION,  "typeof String.prototype['toLowerCase']",  "function", typeof String.prototype['toLowerCase']);
    array[item++] = new TestCase( SECTION,  "typeof String.prototype.toUpperCase",  "function", typeof String.prototype.toUpperCase);
    array[item++] = new TestCase( SECTION,  "typeof String.prototype['toUpperCase']",  "function", typeof String.prototype['toUpperCase']);
    array[item++] = new TestCase( SECTION,  "typeof String.prototype.length",       "undefined",typeof String.prototype.length );
    array[item++] = new TestCase( SECTION,  "typeof String.prototype['length']",       "undefined",typeof String.prototype['length'] );


    // properties of the Boolean object
    array[item++] = new TestCase( SECTION,  "typeof Boolean.prototype",    "object",typeof Boolean.prototype );
    array[item++] = new TestCase( SECTION,  "typeof Boolean['prototype']",    "object",typeof Boolean['prototype'] );
    array[item++] = new TestCase( SECTION,  "typeof Boolean.constructor",  "object",typeof Boolean.constructor );
    array[item++] = new TestCase( SECTION,  "typeof Boolean['constructor']",  "object",typeof Boolean['constructor'] );
    array[item++] = new TestCase( SECTION,  "typeof Boolean.prototype.valueOf",      "function",typeof Boolean.prototype.valueOf );
    array[item++] = new TestCase( SECTION,  "typeof Boolean.prototype['valueOf']",      "function",typeof Boolean.prototype['valueOf']);
    array[item++] = new TestCase( SECTION,  "typeof Boolean.prototype.toString",     "function", typeof Boolean.prototype.toString);
    array[item++] = new TestCase( SECTION,  "typeof Boolean.prototype['toString']",     "function", typeof Boolean.prototype['toString']);

    // properties of the Number object

    array[item++] = new TestCase( SECTION,  "typeof Number.MAX_VALUE",    "number",typeof Number.MAX_VALUE );
    array[item++] = new TestCase( SECTION,  "typeof Number['MAX_VALUE']",    "number",typeof Number['MAX_VALUE']);
    array[item++] = new TestCase( SECTION,  "typeof Number.MIN_VALUE",    "number", typeof Number.MIN_VALUE);
    array[item++] = new TestCase( SECTION,  "typeof Number['MIN_VALUE']",    "number", typeof Number['MIN_VALUE']);
    array[item++] = new TestCase( SECTION,  "typeof Number.NaN",          "number", typeof Number.NaN);
    array[item++] = new TestCase( SECTION,  "typeof Number['NaN']",          "number", typeof Number['NaN']);
    array[item++] = new TestCase( SECTION,  "typeof Number.NEGATIVE_INFINITY",    "number", typeof Number.NEGATIVE_INFINITY);
    array[item++] = new TestCase( SECTION,  "typeof Number['NEGATIVE_INFINITY']",    "number", typeof Number['NEGATIVE_INFINITY']);
    array[item++] = new TestCase( SECTION,  "typeof Number.POSITIVE_INFINITY",    "number", typeof Number.POSITIVE_INFINITY);
    array[item++] = new TestCase( SECTION,  "typeof Number['POSITIVE_INFINITY']",    "number", typeof Number['POSITIVE_INFINITY']);
    array[item++] = new TestCase( SECTION,  "typeof Number.prototype.toString",     "function",typeof Number.prototype.toString );
    array[item++] = new TestCase( SECTION,  "typeof Number.prototype['toString']",     "function",typeof Number.prototype['toString'] );
    array[item++] = new TestCase( SECTION,  "typeof Number.prototype.constructor",  "object", typeof Number.prototype.constructor);
    array[item++] = new TestCase( SECTION,  "typeof Number.prototype['constructor']",  "object", typeof Number.prototype['constructor']);
    array[item++] = new TestCase( SECTION,  "typeof Number.prototype.valueOf",        "function",typeof Number.prototype.valueOf );
        array[item++] = new TestCase( SECTION,  "typeof Number.prototype['valueOf']",        "function",typeof Number.prototype['valueOf'] );

    // properties of the Math Object.
    array[item++] = new TestCase( SECTION,  "typeof Math.E",        "number", typeof Math.E);
    array[item++] = new TestCase( SECTION,  "typeof Math['E']",        "number", typeof Math['E']);
    array[item++] = new TestCase( SECTION,  "typeof Math.LN10",     "number", typeof Math.LN10);
        array[item++] = new TestCase( SECTION,  "typeof Math['LN10']",     "number", typeof Math['LN10']);
    array[item++] = new TestCase( SECTION,  "typeof Math.LN2",      "number", typeof Math.LN2);
        array[item++] = new TestCase( SECTION,  "typeof Math['LN2']",      "number", typeof Math['LN2']);
    array[item++] = new TestCase( SECTION,  "typeof Math.LOG2E",    "number", typeof Math.LOG2E);
        array[item++] = new TestCase( SECTION,  "typeof Math['LOG2E']",    "number", typeof Math['LOG2E']);
    array[item++] = new TestCase( SECTION,  "typeof Math.LOG10E",   "number", typeof Math.LOG10E);
        array[item++] = new TestCase( SECTION,  "typeof Math['LOG10E']",   "number", typeof Math['LOG10E']);
    array[item++] = new TestCase( SECTION,  "typeof Math.PI",       "number", typeof Math.PI);
        array[item++] = new TestCase( SECTION,  "typeof Math['PI']",       "number", typeof Math['PI']);
    array[item++] = new TestCase( SECTION,  "typeof Math.SQRT1_2",  "number", typeof Math.SQRT1_2);
        array[item++] = new TestCase( SECTION,  "typeof Math['SQRT1_2']",  "number", typeof Math['SQRT1_2']);
    array[item++] = new TestCase( SECTION,  "typeof Math.SQRT2",    "number", typeof Math.SQRT2);
        array[item++] = new TestCase( SECTION,  "typeof Math['SQRT2']",    "number", typeof Math['SQRT2']);
    array[item++] = new TestCase( SECTION,  "typeof Math.abs",      "function", typeof Math.abs);
        array[item++] = new TestCase( SECTION,  "typeof Math['abs']",      "function", typeof Math['abs']);
    array[item++] = new TestCase( SECTION,  "typeof Math.acos",     "function", typeof Math.acos);
        array[item++] = new TestCase( SECTION,  "typeof Math['acos']",     "function", typeof Math['acos']);
    array[item++] = new TestCase( SECTION,  "typeof Math.asin",     "function", typeof Math.asin);
        array[item++] = new TestCase( SECTION,  "typeof Math['asin']",     "function", typeof Math['asin']);
    array[item++] = new TestCase( SECTION,  "typeof Math.atan",     "function", typeof Math.atan);
        array[item++] = new TestCase( SECTION,  "typeof Math['atan']",     "function", typeof Math['atan']);
    array[item++] = new TestCase( SECTION,  "typeof Math.atan2",    "function", typeof Math.atan2);
        array[item++] = new TestCase( SECTION,  "typeof Math['atan2']",    "function", typeof Math['atan2']);
    array[item++] = new TestCase( SECTION,  "typeof Math.ceil",     "function", typeof Math.ceil);
        array[item++] = new TestCase( SECTION,  "typeof Math['ceil']",     "function", typeof Math['ceil']);
    array[item++] = new TestCase( SECTION,  "typeof Math.cos",      "function", typeof Math.cos);
        array[item++] = new TestCase( SECTION,  "typeof Math['cos']",      "function", typeof Math['cos']);
    array[item++] = new TestCase( SECTION,  "typeof Math.exp",      "function", typeof Math.exp);
        array[item++] = new TestCase( SECTION,  "typeof Math['exp']",      "function", typeof Math['exp']);
    array[item++] = new TestCase( SECTION,  "typeof Math.floor",    "function", typeof Math.floor);
        array[item++] = new TestCase( SECTION,  "typeof Math['floor']",    "function", typeof Math['floor']);
    array[item++] = new TestCase( SECTION,  "typeof Math.log",      "function", typeof Math.log);
        array[item++] = new TestCase( SECTION,  "typeof Math['log']",      "function", typeof Math['log']);
    array[item++] = new TestCase( SECTION,  "typeof Math.max",      "function", typeof Math.max);
        array[item++] = new TestCase( SECTION,  "typeof Math['max']",      "function", typeof Math['max']);
    array[item++] = new TestCase( SECTION,  "typeof Math.min",      "function", typeof Math.min);
        array[item++] = new TestCase( SECTION,  "typeof Math['min']",      "function", typeof Math['min']);
    array[item++] = new TestCase( SECTION,  "typeof Math.pow",      "function", typeof Math.pow);
        array[item++] = new TestCase( SECTION,  "typeof Math['pow']",      "function", typeof Math['pow']);
    array[item++] = new TestCase( SECTION,  "typeof Math.random",   "function", typeof Math.random);
        array[item++] = new TestCase( SECTION,  "typeof Math['random']",   "function", typeof Math['random']);
    array[item++] = new TestCase( SECTION,  "typeof Math.round",    "function", typeof Math.round);
        array[item++] = new TestCase( SECTION,  "typeof Math['round']",    "function", typeof Math['round']);
    array[item++] = new TestCase( SECTION,  "typeof Math.sin",      "function", typeof Math.sin);
        array[item++] = new TestCase( SECTION,  "typeof Math['sin']",      "function", typeof Math['sin']);
    array[item++] = new TestCase( SECTION,  "typeof Math.sqrt",     "function", typeof Math.sqrt);
        array[item++] = new TestCase( SECTION,  "typeof Math['sqrt']",     "function", typeof Math['sqrt']);
    array[item++] = new TestCase( SECTION,  "typeof Math.tan",      "function", typeof Math.tan);
       array[item++] = new TestCase( SECTION,  "typeof Math['tan']",      "function", typeof Math['tan']);

    // properties of the Date object
    array[item++] = new TestCase( SECTION,  "typeof Date.parse",        "function", typeof Date.parse);
    array[item++] = new TestCase( SECTION,  "typeof Date['parse']",        "function", typeof Date['parse']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype",    "object", typeof Date.prototype);
    array[item++] = new TestCase( SECTION,  "typeof Date['prototype']",    "object", typeof Date['prototype']);
    array[item++] = new TestCase( SECTION,  "typeof Date.UTC",          "function", typeof Date.UTC);
    array[item++] = new TestCase( SECTION,  "typeof Date['UTC']",          "function", typeof Date['UTC']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.constructor",    "object", typeof Date.prototype.constructor);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['constructor']",    "object", typeof Date.prototype['constructor']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.toString",       "function", typeof Date.prototype.toString);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['toString']",       "function", typeof Date.prototype['toString']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.valueOf",        "function", typeof Date.prototype.valueOf);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['valueOf']",        "function", typeof Date.prototype['valueOf']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getTime",        "function", typeof Date.prototype.getTime);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getTime']",        "function", typeof Date.prototype['getTime']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getYear",        "undefined", typeof Date.prototype.getYear);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getYear']",        "undefined", typeof Date.prototype['getYear']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getFullYear",    "function", typeof Date.prototype.getFullYear);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getFullYear']",    "function", typeof Date.prototype['getFullYear']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getUTCFullYear", "function", typeof Date.prototype.getUTCFullYear);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getUTCFullYear']", "function", typeof Date.prototype['getUTCFullYear']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getMonth",       "function", typeof Date.prototype.getMonth);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getMonth']",       "function", typeof Date.prototype['getMonth']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getUTCMonth",    "function", typeof Date.prototype.getUTCMonth);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getUTCMonth']",    "function", typeof Date.prototype['getUTCMonth']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getDate",        "function", typeof Date.prototype.getDate);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getDate']",        "function", typeof Date.prototype['getDate']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getUTCDate",     "function", typeof Date.prototype.getUTCDate);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getUTCDate']",     "function", typeof Date.prototype['getUTCDate']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getDay",         "function", typeof Date.prototype.getDay);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getDay']",         "function", typeof Date.prototype['getDay']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getUTCDay",      "function", typeof Date.prototype.getUTCDay);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getUTCDay']",      "function", typeof Date.prototype['getUTCDay']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getHours",       "function", typeof Date.prototype.getHours);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getHours']",       "function", typeof Date.prototype['getHours']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getUTCHours",    "function", typeof Date.prototype.getUTCHours);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getUTCHours']",    "function", typeof Date.prototype['getUTCHours']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getMinutes",     "function", typeof Date.prototype.getMinutes);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getMinutes']",     "function", typeof Date.prototype['getMinutes']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getUTCMinutes",  "function", typeof Date.prototype.getUTCMinutes);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getUTCMinutes']",  "function", typeof Date.prototype['getUTCMinutes']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getSeconds",     "function", typeof Date.prototype.getSeconds);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getSeconds']",     "function", typeof Date.prototype['getSeconds']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getUTCSeconds",  "function", typeof Date.prototype.getUTCSeconds);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getUTCSeconds']",  "function", typeof Date.prototype['getUTCSeconds']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getMilliseconds","function", typeof Date.prototype.getMilliseconds);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getMilliseconds']","function", typeof Date.prototype['getMilliseconds']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.getUTCMilliseconds", "function", typeof Date.prototype.getUTCMilliseconds);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['getUTCMilliseconds']", "function", typeof Date.prototype['getUTCMilliseconds']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setTime",        "function", typeof Date.prototype.setTime);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setTime']",        "function", typeof Date.prototype['setTime']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setMilliseconds","function", typeof Date.prototype.setMilliseconds);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setMilliseconds']","function", typeof Date.prototype['setMilliseconds']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setUTCMilliseconds", "function", typeof Date.prototype.setUTCMilliseconds);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setUTCMilliseconds']", "function", typeof Date.prototype['setUTCMilliseconds']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setSeconds",     "function", typeof Date.prototype.setSeconds);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setSeconds']",     "function", typeof Date.prototype['setSeconds']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setUTCSeconds",  "function", typeof Date.prototype.setUTCSeconds);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setUTCSeconds']",  "function", typeof Date.prototype['setUTCSeconds']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setMinutes",     "function", typeof Date.prototype.setMinutes);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setMinutes']",     "function", typeof Date.prototype['setMinutes']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setUTCMinutes",  "function", typeof Date.prototype.setUTCMinutes);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setUTCMinutes']",  "function", typeof Date.prototype['setUTCMinutes']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setHours",       "function", typeof Date.prototype.setHours);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setHours']",       "function", typeof Date.prototype['setHours']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setUTCHours",    "function", typeof Date.prototype.setUTCHours);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setUTCHours']",    "function", typeof Date.prototype['setUTCHours']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setDate",        "function", typeof Date.prototype.setDate);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setDate']",        "function", typeof Date.prototype['setDate']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setUTCDate",     "function", typeof Date.prototype.setUTCDate);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setUTCDate']",     "function", typeof Date.prototype['setUTCDate']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setMonth",       "function", typeof Date.prototype.setMonth);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setMonth']",       "function", typeof Date.prototype['setMonth']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setUTCMonth",    "function", typeof Date.prototype.setUTCMonth);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setUTCMonth']",    "function", typeof Date.prototype['setUTCMonth']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setFullYear",    "function", typeof Date.prototype.setFullYear);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setFullYear']",    "function", typeof Date.prototype['setFullYear']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setUTCFullYear", "function", typeof Date.prototype.setUTCFullYear);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setUTCFullYear']", "function", typeof Date.prototype['setUTCFullYear']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.setYear",        "undefined", typeof Date.prototype.setYear);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['setYear']",        "undefined", typeof Date.prototype['setYear']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.toLocaleString", "function", typeof Date.prototype.toLocaleString);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['toLocaleString']", "function", typeof Date.prototype['toLocaleString']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.toUTCString",    "function", typeof Date.prototype.toUTCString);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['toUTCString']",    "function", typeof Date.prototype['toUTCString']);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype.toGMTString",    "undefined", typeof Date.prototype.toGMTString);
    array[item++] = new TestCase( SECTION,  "typeof Date.prototype['toGMTString']",    "undefined", typeof Date.prototype['toGMTString']);

    for ( var i = 0, RESULT; i < PROPERTY.length; i++ ) {
       RESULT = typeof (PROPERTY[i].object.PROPERTY[i].name);


        array[item++] = new TestCase( SECTION,
                                        "typeof " + PROPERTY[i].object + "." + PROPERTY[i].name,
                                        PROPERTY[i].type+"",
                                        RESULT );


	RESULT = typeof (PROPERTY[i].object['PROPERTY[i].name']);


        array[item++] = new TestCase( SECTION,
                                        "typeof " + PROPERTY[i].object + "['" + PROPERTY[i].name +"']",
                                        PROPERTY[i].type,
                                        RESULT );

    }
    
    //restore.  deleted the extra property created so it doesn't interfere with another testcase
    //in ATS.
    delete Function.prototype.myProperty;
    return array;
}

function MyObject( arg0, arg1, arg2, arg3, arg4 ) {
    this.name   = arg0;
}
function Property( object, name, type ) {
    this.object = object;
    this.name = name;
    this.type = type;
}
