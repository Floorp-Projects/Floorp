/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 352291;
var summary = 'disassembly of regular expression';
var actual = '';
var expect = 'TypeError: /g/g is not a function';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  if (typeof dis != 'function')
  {
    actual = expect = 'disassembly not supported, test skipped.';
  }
  else
  {
    try
    {
      dis(/g/g)
    }
    catch(ex)
    {
      actual = ex + '';
    }
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
