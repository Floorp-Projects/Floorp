/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 473040;
var summary = 'Do not assert: tm->reservedDoublePoolPtr > tm->reservedDoublePool';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 

Object.defineProperty(__proto__, "functional",
{
  enumerable: true, configurable: true,
  get: new Function("gc()")
});
for each (let x in [new Boolean(true), new Boolean(true), -0, new
                    Boolean(true), -0]) { undefined; }


reportCompare(expect, actual, summary);
