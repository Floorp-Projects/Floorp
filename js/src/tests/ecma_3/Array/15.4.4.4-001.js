/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    19 September 2002
 * SUMMARY: Testing Array.prototype.concat()
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=169795
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 169795;
var summary = 'Testing Array.prototype.concat()';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];
var x;


status = inSection(1);
x = "Hello";
actual = [].concat(x).toString();
expect = x.toString();
addThis();

status = inSection(2);
x = 999;
actual = [].concat(x).toString();
expect = x.toString();
addThis();

status = inSection(3);
x = /Hello/g;
actual = [].concat(x).toString();
expect = x.toString();
addThis();

status = inSection(4);
x = new Error("Hello");
actual = [].concat(x).toString();
expect = x.toString();
addThis();

status = inSection(5);
x = function() {return "Hello";};
actual = [].concat(x).toString();
expect = x.toString();
addThis();

status = inSection(6);
x = [function() {return "Hello";}];
actual = [].concat(x).toString();
expect = x.toString();
addThis();

status = inSection(7);
x = [1,2,3].concat([4,5,6]);
actual = [].concat(x).toString();
expect = x.toString();
addThis();

status = inSection(8);
x = eval('this');
actual = [].concat(x).toString();
expect = x.toString();
addThis();

/*
 * The next two sections are by igor@icesoft.no; see
 * http://bugzilla.mozilla.org/show_bug.cgi?id=169795#c3
 */
status = inSection(9);
x={length:0};
actual = [].concat(x).toString();
expect = x.toString();
addThis();

status = inSection(10);
x={length:2, 0:0, 1:1};
actual = [].concat(x).toString();
expect = x.toString();
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
