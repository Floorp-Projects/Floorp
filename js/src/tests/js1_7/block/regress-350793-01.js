/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 350793;
var summary = 'for-in loops must be yieldable';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  function foo()
  {
    function gen() { for(let itty in [5,6,7,8]) yield ({ }); }

    iter = gen();

    let count = 0;
    for each(let _ in iter)
      ++count;
  }

  foo();

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
