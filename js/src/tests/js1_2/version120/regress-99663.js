// |reftest| skip -- obsolete test
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 99663;
var summary = 'Regression test for Bugzilla bug 99663';
/*
 * This testcase expects error messages containing
 * the phrase 'read-only' or something similar -
 */
var READONLY = /read\s*-?\s*only/;
var READONLY_TRUE = 'a "read-only" error';
var READONLY_FALSE = 'Error: ';
var FAILURE = 'NO ERROR WAS GENERATED!';
var status = '';
var actual = '';
var expect= '';
var statusitems = [];
var expectedvalues = [];
var actualvalues = [];


/*
 * These MUST be compiled in JS1.2 or less for the test to work - see above
 */
function f1()
{
  with (it)
  {
    for (rdonly in this);
  }
}


function f2()
{
  for (it.rdonly in this);
}


function f3(s)
{
  for (it[s] in this);
}



/*
 * Begin testing by capturing actual vs. expected values.
 * Initialize to FAILURE; this will get reset if all goes well -
 */
actual = FAILURE;
try
{
  f1();
}
catch(e)
{
  actual = readOnly(e.message);
}
expect= READONLY_TRUE;
status = 'Section 1 of test - got ' + actual;
addThis();


actual = FAILURE;
try
{
  f2();
}
catch(e)
{
  actual = readOnly(e.message);
}
expect= READONLY_TRUE;
status = 'Section 2 of test - got ' + actual;
addThis();


actual = FAILURE;
try
{
  f3('rdonly');
}
catch(e)
{
  actual = readOnly(e.message);
}
expect= READONLY_TRUE;
status = 'Section 3 of test - got ' + actual;
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function readOnly(msg)
{
  if (msg.match(READONLY))
    return READONLY_TRUE;
  return READONLY_FALSE + msg;
}


function addThis()
{
  statusitems[UBound] = status;
  actualvalues[UBound] = actual;
  expectedvalues[UBound] = expect;
  UBound++;
}


function test()
{
  print ('Bug Number ' + bug);
  print ('STATUS: ' + summary);

  for (var i=0; i<UBound; i++)
  {
    writeTestCaseResult(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}
