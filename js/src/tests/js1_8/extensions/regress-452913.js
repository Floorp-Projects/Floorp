/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 452913;
var summary = 'Do not crash with defined getter and for (let)';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
(this.__defineGetter__("x", function (x)'foo'.replace(/o/g, [1].push)));
for(let y in [,,,]) for(let y in [,,,]) x = x;

reportCompare(expect, actual, summary);
