// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 362583;
var summary = 'Do not assert: caller->fun && !JSFUN_HEAVYWEIGHT_TEST(caller->fun->flags)';
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

  if (typeof Script == 'undefined')
  {
    expect = actual = 'Script object not defined, test skipped.';
  }
  else
  {
    try
    {
      this.x setter= (new Script(''));
      this.watch('x', function() { return; import p.q; });
      x = 4;
    }
    catch(ex)
    {
    }
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
