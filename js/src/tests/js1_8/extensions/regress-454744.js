/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 454744;
var summary = 'Do not assert with JIT: PCVAL_IS_SPROP(entry->vword)';
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


  try
  {
    this.__defineGetter__('x', function() 2); for (var j=0;j<4;++j) { x=1; }
  }
  catch(ex)
  {
  }


  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
