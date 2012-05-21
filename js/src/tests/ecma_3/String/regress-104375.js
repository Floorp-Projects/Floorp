/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 12 October 2001
 *
 * SUMMARY: Regression test for string.replace bug 104375
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=104375
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 104375;
var summary = 'Testing string.replace() with backreferences';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * Use the regexp to replace 'uid=31' with 'uid=15'
 *
 * In the second parameter of string.replace() method,
 * "$1" refers to the first backreference: 'uid='
 */
var str = 'uid=31';
var re = /(uid=)(\d+)/;

// try the numeric literal 15
status = inSection(1);
actual  = str.replace (re, "$1" + 15);
expect = 'uid=15';
addThis();

// try the string literal '15'
status = inSection(2);
actual  = str.replace (re, "$1" + '15');
expect = 'uid=15';
addThis();

// try a letter before the '15'
status = inSection(3);
actual  = str.replace (re, "$1" + 'A15');
expect = 'uid=A15';
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}


