/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 343966;
var summary = 'ClearScope foo regressed due to bug 343417';
var actual = 'failed';
var expect = 'passed';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  Function["prototype"].inherits=function(a){};
  function foo(){};
  function bar(){};
  foo.inherits(bar);
  actual = "passed";

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
