/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 345855;
var summary = 'Blank yield expressions are not syntax errors';
var actual = '';
var expect = 'No Error';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = "SyntaxError";
  try
  {
    eval('(function() {x = 12 + yield;})');
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex.name;
  }
  reportCompare(expect, actual, summary + ': function() {x = 12 + yield;}');

  expect = "SyntaxError";
  try
  {
    eval('(function() {x = 12 + yield 42})');
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex.name;
  }
  reportCompare(expect, actual, summary + ': function() {x = 12 + yield 42}');

  expect = 'No Error';
  try
  {
    eval('(function() {x = 12 + (yield);})');
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': function() {x = 12 + (yield);}');

  try
  {
    eval('(function () {foo((yield))})');
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': function () {foo((yield))}');

  try
  {
    eval('(function() {x = 12 + (yield 42)})');
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': function() {x = 12 + (yield 42)}');

  try
  {
    eval('(function (){foo((yield 42))})');
    actual = 'No Error';
  }
  catch(ex)
  {
    actual = ex + '';
  }
  reportCompare(expect, actual, summary + ': function (){foo((yield 42))}');

  exitFunc ('test');
}
