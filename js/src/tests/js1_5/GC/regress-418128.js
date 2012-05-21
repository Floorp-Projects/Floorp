/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 418128;
var summary = 'GC hazard with ++/--';
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
 
  var obj = {};
  var id = { toString: function() { return ""+Math.pow(2, 0.1); } }
  obj[id] = { valueOf: unrooter };
  print(obj[id]++);
  gc();
  print(uneval(obj));

  function unrooter()
  {
    delete obj[id];
    gc();
    return 10;
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
