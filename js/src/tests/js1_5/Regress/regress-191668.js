/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    03 February 2003
 * SUMMARY: Testing script containing <!- at internal buffer boundary.
 * JS parser must look for HTML comment-opener <!--, but mustn't disallow <!-
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=191668
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 191668;
var summary = 'Testing script containing <!- at internal buffer boundary';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

var N = 512;
var j = 0;
var str = 'if (0<!-0) ++j;';

for (var i=0; i!=N; ++i)
{
  eval(str);
  str = ' ' + str;
}

status = inSection(1);
actual = j;
expect = N;
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
