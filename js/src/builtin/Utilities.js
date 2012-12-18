/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*jshint bitwise: true, camelcase: false, curly: false, eqeqeq: true, forin: true,
         immed: true, indent: 4, latedef: false, newcap: false, noarg: true,
         noempty: true, nonew: true, plusplus: false, quotmark: false, regexp: true,
         undef: true, unused: false, strict: false, trailing: true,
*/

/*global ToObject: false, ToInteger: false, IsCallable: false, ThrowError: false,
         AssertionFailed: false, MakeConstructible: false, DecompileArg: false,
         callFunction: false,
         IS_UNDEFINED: false, TO_UINT32: false,
         JSMSG_NOT_FUNCTION: false, JSMSG_MISSING_FUN_ARG: false,
         JSMSG_EMPTY_ARRAY_REDUCE: false,
*/


/* cache built-in functions before applications can change them */
var std_ArrayIndexOf = ArrayIndexOf;
var std_ArrayJoin = Array.prototype.join;
var std_ArrayPush = Array.prototype.push;
var std_ArraySlice = Array.prototype.slice;
var std_ArraySort = Array.prototype.sort;
var std_ObjectCreate = Object.create;
var std_String = String;


/********** List specification type **********/


/* Spec: ECMAScript Language Specification, 5.1 edition, 8.8 */
function List() {
    if (IS_UNDEFINED(List.prototype)) {
        var proto = std_ObjectCreate(null);
        proto.indexOf = std_ArrayIndexOf;
        proto.join = std_ArrayJoin;
        proto.push = std_ArrayPush;
        proto.slice = std_ArraySlice;
        proto.sort = std_ArraySort;
        List.prototype = proto;
    }
}
MakeConstructible(List);


/********** Record specification type **********/


/* Spec: ECMAScript Internationalization API Specification, draft, 5 */
function Record() {
    return std_ObjectCreate(null);
}
MakeConstructible(Record);


/********** Abstract operations defined in ECMAScript Language Specification **********/


/* Spec: ECMAScript Language Specification, 5.1 edition, 8.12.6 and 11.8.7 */
function HasProperty(o, p) {
    return p in o;
}


/* Spec: ECMAScript Language Specification, 5.1 edition, 9.2 and 11.4.9 */
function ToBoolean(v) {
    return !!v;
}


/* Spec: ECMAScript Language Specification, 5.1 edition, 9.3 and 11.4.6 */
function ToNumber(v) {
    return +v;
}


/* Spec: ECMAScript Language Specification, 5.1 edition, 9.8 and 15.2.1.1 */
function ToString(v) {
    assert(arguments.length > 0, "__toString");
    return std_String(v);
}


/********** Assertions **********/


function assert(b, info) {
    if (!b)
        AssertionFailed(info);
}

