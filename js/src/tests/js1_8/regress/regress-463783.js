/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 463783;
var summary = 'TM: Do not assert: "need a way to EOT now, since this is trace end": 0';
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
 
  jit(true);

  var a = [0,1,2,3,4,5];
  for (let f in a);
  [(function(){})() for each (x in a) if (x)];

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
