/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    29 April 2003
 * SUMMARY: Testing merged if-clauses
 *
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=203841
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 203841;
var summary = 'Testing merged if-clauses';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


status = inSection(1);
var a = 0;
var b = 0;
var c = 0;
if (a == 5, b == 6) { c = 1; }
actual = c;
expect = 0;
addThis();

status = inSection(2);
a = 5;
b = 0;
c = 0;
if (a == 5, b == 6) { c = 1; }
actual = c;
expect = 0;
addThis();

status = inSection(3);
a = 5;
b = 6;
c = 0;
if (a == 5, b == 6) { c = 1; }
actual = c;
expect = 1;
addThis();

/*
 * Now get tricky and use the = operator inside the if-clause
 */
status = inSection(4);
a = 0;
b = 6;
c = 0;
if (a = 5, b == 6) { c = 1; }
actual = c;
expect = 1;
addThis();

status = inSection(5);
c = 0;
if (1, 1 == 6) { c = 1; }
actual = c;
expect = 0;
addThis();


/*
 * Now some tests involving arrays
 */
var x=[];

status = inSection(6); // get element case
c = 0;
if (x[1==2]) { c = 1; }
actual = c;
expect = 0;
addThis();

status = inSection(7); // set element case
c = 0;
if (x[1==2]=1) { c = 1; }
actual = c;
expect = 1;
addThis();

status = inSection(8); // delete element case
c = 0;
if (delete x[1==2]) { c = 1; }
actual = c;
expect = 1;
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
