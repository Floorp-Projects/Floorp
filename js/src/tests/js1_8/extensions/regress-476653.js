/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 476653;
var summary = 'Do not crash @ QuoteString';
var actual = '';
var expect = '';


printBugNumber(BUGNUMBER);
printStatus (summary);

jit(true);

for each (let x1 in ['']) 
for (i = 0; i < 1; ++i) {}
delete uneval;
for (i = 0; i < 1; ++i) {}
for each (let x in [new String('q'), '', /x/, '', /x/]) { 
  for (var y = 0; y < 7; ++y) { if (y == 2 || y == 6) { setter = x; } } 
}
try
{
  this.f(z);
}
catch(ex)
{
}

jit(false);

reportCompare(expect, actual, summary);

