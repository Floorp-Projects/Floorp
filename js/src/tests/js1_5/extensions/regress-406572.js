/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 406572;
var summary = 'JSOP_CLOSURE unconditionally replaces properties of the variable object - Browser only';
var actual = '';
var expect = '';

printBugNumber(BUGNUMBER);
printStatus (summary);

if (typeof window != 'undefined')
{
  var s = self;

  document.writeln(uneval(self));
  self = 1;
  document.writeln(uneval(self));

  if (1)
    function self() { return 1; }

  document.writeln(uneval(self));

  // The test harness might rely on self having its original value: restore it.
  self = s;
}
else
{
  expect = actual = 'Test can only run in a Gecko 1.9 browser or later.';
  print(actual);
}
reportCompare(expect, actual, summary);


