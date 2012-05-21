/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 351503;
var summary = 'decompilation of TypeError messages';
var actual = '';
var expect = '';

test();

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expect = 'TypeError: can\'t convert Object to string';
  try
  {
    function p() { return { toString: null } } "fafa".replace(/a/g, p);
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 1');

  expect = 'TypeError: can\'t convert Object to string';
  try
  {
    a=1; b=2; c={toString: null}; "hahbhc".replace(/[abc]/g, eval);
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 2');

  expect = 'TypeError: can\'t convert ({toString:(function () {yield 4;})}) to primitive type';
  try
  {
    3 + ({toString:function(){yield 4} });
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 3');

  expect = "TypeError: can't convert ({toString:{}}) to primitive type";
  try
  {
    3 + ({toString:({}) }) ;
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 4');

  expect = 'TypeError: 3 is not a function';
  try
  {
    ([5].map)(3);
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 5');

  expect = 'TypeError: 5 is not a function';
  try
  {
    (1 + (4))();
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': 6');
}
