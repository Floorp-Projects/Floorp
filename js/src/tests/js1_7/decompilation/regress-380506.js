/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 380506;
var summary = 'Decompilation of nested-for and for-if comprehensions';
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
 
  var f = function (){return [i*i for(i in [0]) if (i%2)]};
  expect = 'function (){return [i*i for(i in [0]) if (i%2)];}';
  actual = f + '';
  compareSource(expect, actual, summary);

  f = function (){return [i*j for(i in [0]) for (j in [1])]};
  expect = 'function (){return [i*j for(i in [0]) for (j in [1])];}';
  actual = f + '';
  compareSource(expect, actual, summary);

  exitFunc ('test');
}
