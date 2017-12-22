/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 313803;
var summary = 'uneval() on func with embedded objects with getter or setter';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

var func = function ff() {
  obj = { get foo() { return "foo"; }};
  return 1;
};

actual = uneval(func);

expect = '(function ff() {\n\
  obj = { get foo() { return "foo"; }};\n\
  return 1;\n\
})';

compareSource(expect, actual, summary);
