/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 427798;
var summary = 'js_PutBlockObject is slow';
var actual = 'Good result';
var expect = 'Good result';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  function f(N)
  {
    for (var i = 0; i != N; ++i) {
      var f, g;
      {
        let j = i + 1;
        f = function() { j += 2; return j; }
        if (f() != i + 3)
          throw "Bad result";
      }
      if (f() != i + 5)
        throw "Bad result";
      {
        let j = i + 1, k = i + 2;
        f = function() { j += 2; k++; return j + k; }
        if (f() != i + i + 6)
          throw "Bad result";
      }
      if (f() != i + i + 9)
        throw "Bad result";
    }
  }

  try
  {
    f(20*1000);
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
