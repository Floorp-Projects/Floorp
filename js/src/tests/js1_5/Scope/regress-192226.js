/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    07 February 2003
 * SUMMARY: Testing a nested function call under |with| or |catch|
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=192226
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 192226;
var summary = 'Testing a nested function call under |with| or |catch|';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var counter = 0;


function f()
{
  try
  {
    with (Math)
    {
      test0();
      test1(sin);
    }
    throw 1;
  }
  catch (e)
  {
    test0();
    test1(e);
  }
}

function test0()
{
  ++counter;
}

function test1(arg)
{
  ++counter;
}


status = inSection(1);
f(); // sets |counter|
actual = counter;
expect = 4;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
