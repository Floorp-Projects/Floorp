/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = 'Do not assert: v.isString()';
var BUGNUMBER = 477053;
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);

try
{
    function f() { eval("with(arguments)throw [];"); }
    f();
}
catch(ex)
{
}

reportCompare(expect, actual);

