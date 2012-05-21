/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352873;
var summary = 'decompilation of nested |try...catch| with |with|';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var f;

  f = function() { try { with({}) { try { } finally { return; }  }  } finally { } }
  expect = 'function() { try { with({}) { try { } finally { return; }  }  } finally { } }';
  actual = f + '';
  compareSource(expect, actual, summary);

  expect = actual = 'No Crash';
  f();
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
