/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 482263;
var summary = 'TM: Do not assert: x->oprnd2() == lirbuf->sp || x->oprnd2() == gp_ins';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);


Object.defineProperty(__proto__, "x",
{
  enumerable: true, configurable: true,
  get: function () { return ([]) }
});
for (let x of []) { for (let x of ['', '']) { } }


reportCompare(expect, actual, summary);

delete __proto__.x;
