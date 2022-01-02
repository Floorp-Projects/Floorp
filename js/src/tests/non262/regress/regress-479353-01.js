/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 479353;
var summary = 'Do not assert: (uint32_t)(index_) < atoms_->length';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);
 
function f(code)
{
  (eval("(function(){" + code + "});"))();
}
x = {};
f("y = this;");
f("x, y; for (let x of [arguments]) {}");

reportCompare(expect, actual, summary);
