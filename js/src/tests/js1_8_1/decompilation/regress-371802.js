/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 371802;
var summary = 'Do not assert with group assignment';
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

  var f = (function (a,b,n){for(var [i,j]=[a,b];i<n;[i,j]=[a,b])print(i)});
  expect = 'function (a,b,n){for(var [i,j]=[a,b];i<n;[i,j]=[a,b]){print(i);}}';
  actual = f + '';

  compareSource(expect, actual, summary);

  exitFunc ('test');
}
