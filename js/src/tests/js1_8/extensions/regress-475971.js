// |reftest| skip
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 475971;
var summary = 'js_CheckRedeclaration should unlock object on failures';
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

  if (typeof scatter != 'function')
  {
    print(expect = actual = 
          'Test skipped - requires threaded build with scatter');
  }
  else
  {
    function x() { return 1; };

    // g must run sufficiently long to ensure that the global scope is accessed
    // from the parallel threads.
    function g()
    {
      var sum = 0;
      try {
        for (var i = 0; i != 10000; ++i) {
          sum += x();
        }
      } catch (e) { }
    }

    scatter([g, g]);

    try {
      eval("const x = 1");
    } catch (e) { }

    scatter([g, g]);

    print("Done");
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
