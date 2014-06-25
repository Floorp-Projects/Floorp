/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 344052;
var summary = 'Function prototype - simple shared property';
var actual = '';
var expect = 'true';

Function.prototype.foo = true;
function y(){};

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  actual = String(y.foo);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
