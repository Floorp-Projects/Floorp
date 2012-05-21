// |reftest| random -- bug 524788
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 363258;
var summary = 'Timer resolution';
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

  var start = 0;
  var stop  = 0;
  var i;
  var limit = 0;
  var incr  = 10;
  var resolution = 5;
  
  while (stop - start == 0)
  {
    limit += incr;
    start = Date.now();
    for (i = 0; i < limit; i++) {}
    stop = Date.now();
  }

  print('limit=' + limit + ', resolution=' + resolution + ', time=' + (stop - start));

  expect = true;
  actual = (stop - start <= resolution);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
