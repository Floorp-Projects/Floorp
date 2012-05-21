// |reftest| fails
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 392378;
var summary = 'Regular Expression Non-participating Capture Groups are inaccurate in edge cases';
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
 
  expect = ["", undefined, ""] + '';
  actual = "y".split(/(x)?\1y/) + '';
  reportCompare(expect, actual, summary + ': "y".split(/(x)?\1y/)');

  expect = ["", undefined, ""] + '';
  actual = "y".split(/(x)?y/) + '';
  reportCompare(expect, actual, summary + ': "y".split(/(x)?y/)');

  expect = 'undefined';
  actual = "y".replace(/(x)?\1y/, function($0, $1){ return String($1); }) + '';
  reportCompare(expect, actual, summary + ': "y".replace(/(x)?\\1y/, function($0, $1){ return String($1); })');

  expect = 'undefined';
  actual = "y".replace(/(x)?y/, function($0, $1){ return String($1); }) + '';
  reportCompare(expect, actual, summary + ': "y".replace(/(x)?y/, function($0, $1){ return String($1); })');

  expect = 'undefined';
  actual = "y".replace(/(x)?y/, function($0, $1){ return $1; }) + '';
  reportCompare(expect, actual, summary + ': "y".replace(/(x)?y/, function($0, $1){ return $1; })');

  exitFunc ('test');
}
