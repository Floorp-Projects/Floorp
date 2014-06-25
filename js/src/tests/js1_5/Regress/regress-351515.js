/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351515;
var summary = 'js17 features must be enabled by version request';
var actual = 'No Error';
var expect = 'No Error';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

yield = 1;
let   = 1;

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  function f(yield, let) { return yield+let; }

  var yield = 1;
  var let = 1;

  function yield() {}

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
