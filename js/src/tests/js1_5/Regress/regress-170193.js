/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    22 Sep 2002
 * SUMMARY: adding prop after middle-delete of function w duplicate formal args
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=170193
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 170193;
var summary = 'adding property after middle-delete of function w duplicate formal args';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];

/*
 * This sequence of steps used to cause the SpiderMonkey shell to hang -
 */
function f(a,a,b){}
f.c=42;
f.d=43;
delete f.c;  // "middle delete"
f.e=44;

status = inSection(1);
actual = f.c;
expect = undefined;
addThis();

status = inSection(2);
actual = f.d;
expect = 43;
addThis();

status = inSection(3);
actual = f.e;
expect = 44;
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
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }
}
