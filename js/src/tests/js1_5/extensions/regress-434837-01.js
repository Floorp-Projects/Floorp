/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 434837;
var summary = '|this| in accessors in prototype chain of array';
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
 
  try
  {
    expect = true;
    actual = null;
    x = [ "one", "two" ];
    Array.prototype.__defineGetter__('test1', function() { actual = (this === x); });
    x.test1;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': x.test1');

  try
  {
    expect = false;
    actual = null;
    Array.prototype.__defineGetter__('test2', function() { actual = (this === Array.prototype) });
    x.test2;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': x.test2');

  Array.prototype.__defineGetter__('test3', function() { actual = (this === x) });
  Array.prototype.__defineSetter__('test3', function() { actual = (this === x) });

  try
  {
    expect = true;
    actual = null;
    x.test3;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': x.test3 (1)');

  try
  {
    expect = true;
    actual = null;
    x.test3 = 5;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': x.test3 = 5');

  try
  {
    expect = true;
    actual = null;
    x.test3;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': x.test3 (2)');

  exitFunc ('test');
}
