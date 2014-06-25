/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 488995;
var summary = 'Do not crash with watch, __defineSetter__ on svg';
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

  if (typeof document == 'undefined')
  {
    print('Test skipped: requires browser.');
  }
  else
  {
    try
    {
      var o=document.createElementNS("http://www.w3.org/2000/svg", "svg");
      var p=o.y;
      p.watch('animVal', function(id, oldvar, newval) {} );
      p.type='xxx';
      p.__defineSetter__('animVal', function() {});
      p.animVal=0;
    }
    catch(ex)
    {
    }
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
