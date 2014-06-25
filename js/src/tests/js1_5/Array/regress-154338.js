/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * Date:    26 June 2002
 * SUMMARY: Testing array.join() when separator is a variable, not a literal
 * See http://bugzilla.mozilla.org/show_bug.cgi?id=154338
 *
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = 154338;
var summary = 'Test array.join() when separator is a variable, not a literal';
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


/*
 * Note that x === 'H' and y === 'ome'.
 *
 * Yet for some reason, using |x| or |y| as the separator
 * in arr.join() was causing out-of-memory errors, whereas
 * using the literals 'H', 'ome' was not -
 *
 */
var x = 'Home'[0];
var y = ('Home'.split('H'))[1];


status = inSection(1);
var arr = Array('a', 'b');
actual = arr.join('H');
expect = 'aHb';
addThis();

status = inSection(2);
arr = Array('a', 'b');
actual = arr.join(x);
expect = 'aHb';
addThis();

status = inSection(3);
arr = Array('a', 'b');
actual = arr.join('ome');
expect = 'aomeb';
addThis();

status = inSection(4);
arr = Array('a', 'b');
actual = arr.join(y);
expect = 'aomeb';
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
