/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 462407;
var summary = 'Do not assert: !ti->stackTypeMap.matches(ti_other->stackTypeMap)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);


(function f() { for each (let i in [0, {}, 0, 1.5, {}, 0, 1.5, 0, 0]) { }})();


reportCompare(expect, actual, summary);
