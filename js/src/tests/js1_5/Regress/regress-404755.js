// |reftest| skip -- unreliable - based on GC timing
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 404755;
var summary = 'Do not consume heap when deleting property';
var actual = 'No leak';
var expect = 'No leak';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var n = 1 << 22;
  var o = {};
  do {
    o[0] = 0;
    delete o[0]; 
  } while (--n != 0);

  gc();
  var time = Date.now();
  gc();
  time = Date.now() - time;

  o = {};
  o[0] = 0;
  delete o[0]; 
  gc();
  var time2 = Date.now();
  gc();
  time2 = Date.now() - time2;
  print(time+" "+time2);
  if (time > 2 && time > time2 * 5)
    throw "A possible leak is observed";

  reportCompare(expect, actual, summary);
}
