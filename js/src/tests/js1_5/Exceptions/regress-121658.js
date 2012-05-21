/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    24 Jan 2002
 * SUMMARY: "Too much recursion" errors should be safely caught by try...catch
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=121658
 *
 * In the cases below, we expect i>0. The bug was filed because we
 * were getting i===0; i.e. |i| did not retain the value it had at the
 * location of the error.
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 121658;
var msg = '"Too much recursion" errors should be safely caught by try...catch';
var TEST_PASSED = 'i retained the value it had at location of error';
var TEST_FAILED = 'i did NOT retain this value';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var i;


function f()
{
  ++i;

  // try...catch should catch the "too much recursion" error to ensue
  try
  {
    f();
  }
  catch(e)
  {
  }
}

i=0;
f();
status = inSection(1);
actual = (i>0);
expect = true;
addThis();



// Now try in function scope -
function g()
{
  f();
}

i=0;
g();
status = inSection(2);
actual = (i>0);
expect = true;
addThis();



// Now try in eval scope -
var sEval = 'function h(){++i; try{h();} catch(e){}}; i=0; h();';
eval(sEval);
status = inSection(3);
actual = (i>0);
expect = true;
addThis();



// Try in eval scope and mix functions up -
sEval = 'function a(){++i; try{h();} catch(e){}}; i=0; a();';
eval(sEval);
status = inSection(4);
actual = (i>0);
expect = true;
addThis();




//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = formatThis(actual);
  expectedvalues[UBound] = formatThis(expect);
  UBound++;
}


function formatThis(bool)
{
  return bool? TEST_PASSED : TEST_FAILED;
}


function test()
{
  enterFunc('test');
  printBugNumber(BUGNUMBER);
  printStatus(msg);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
