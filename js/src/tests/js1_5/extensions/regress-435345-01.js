// |reftest| fails
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 435345;
var summary = 'Watch the length property of arrays';
var actual = '';
var expect = '';

// see http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Objects:Object:watch

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
  
  var arr;
 
  try
  {
    expect = 'watcher: propname=length, oldval=0, newval=1; ';
    actual = '';
    arr = [];
    arr.watch('length', watcher);
    arr[0] = '0';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 1');

  try
  {
    expect = 'watcher: propname=length, oldval=1, newval=2; ' + 
      'watcher: propname=length, oldval=2, newval=2; ';
    actual = '';
    arr.push(5);
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 2');

  try
  {
    expect = 'watcher: propname=length, oldval=2, newval=1; ';
    actual = '';
    arr.pop();
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 3');

  try
  {
    expect = 'watcher: propname=length, oldval=1, newval=2; ';
    actual = '';
    arr.length++;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 4');

  try
  {
    expect = 'watcher: propname=length, oldval=2, newval=5; ';
    actual = '';
    arr.length = 5;
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 5');

  exitFunc ('test');
}

function watcher(propname, oldval, newval)
{
  actual += 'watcher: propname=' + propname + ', oldval=' + oldval + 
    ', newval=' + newval + '; ';

  return newval;
}

