/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 355655;
var summary = 'running script can be recompiled';
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

  if (typeof Script == 'undefined')
  {
    print('Test skipped. Script not defined.');
  }
  else
  {
    expect = 'TypeError: cannot compile over a script that is currently executing';
    actual = '';

    try
    {
      t='1';s=Script('s.compile(t);print(t);');s();
    }
    catch(ex)
    {
      actual = ex + '';
    }
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
