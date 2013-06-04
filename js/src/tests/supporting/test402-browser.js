/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Undo at least the damage done by taintArray so that the jstests
 * harness won't die. The properties added to Object.prototype by the various
 * tests have names that are less likely to cause trouble.
 */
setRestoreFunction((function () {
    var Array_indexOf = Array.prototype.indexOf;
    var Array_join = Array.prototype.join;
    var Array_push = Array.prototype.push;
    var Array_slice = Array.prototype.slice;
    var Array_sort = Array.prototype.sort;
    return function () {
        delete Array.prototype["0"];
        Array.prototype.indexOf = Array_indexOf;
        Array.prototype.join = Array_join;
        Array.prototype.push = Array_push;
        Array.prototype.slice = Array_slice;
        Array.prototype.sort = Array_sort;
    };
}()));

/*
 * Loading include files into the browser from a script so that they become
 * synchronously available to that same script is difficult. Instead, request
 * all of them to be loaded before we start.
 */
include("supporting/testBuiltInObject.js");
include("supporting/testIntl.js");
