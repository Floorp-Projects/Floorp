/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Date: 24 September 2001
 *
 * SUMMARY: Truncating arrays that have decimal property names.
 * From correspondence with Igor Bukanov <igor@icesoft.no>:
 */
//-----------------------------------------------------------------------------
var UBound = 0;
var BUGNUMBER = '(none)';
var summary = 'Truncating arrays that have decimal property names';
var BIG_INDEX = 4294967290;
var status = '';
var statusitems = [];
var actual = '';
var actualvalues = [];
var expect= '';
var expectedvalues = [];


var arr = Array(BIG_INDEX);
arr[BIG_INDEX - 1] = 'a';
arr[BIG_INDEX - 10000] = 'b';
arr[BIG_INDEX - 0.5] = 'c';  // not an array index - but a valid property name
// Truncate the array -
arr.length = BIG_INDEX - 5000;


// Enumerate its properties with for..in
var s = '';
for (var i in arr)
{
  s += arr[i];
}


/*
 * We expect s == 'cb' or 'bc' (EcmaScript does not fix the order).
 * Note 'c' is included: for..in includes ALL enumerable properties,
 * not just array-index properties. The bug was: Rhino gave s == ''.
 */
status = inSection(1);
actual = sortThis(s);
expect = 'bc';
addThis();



//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------



function sortThis(str)
{
  var chars = str.split('');
  chars = chars.sort();
  return chars.join('');
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  for (var i=0; i<UBound; i++)
  {
    reportCompare(expectedvalues[i], actualvalues[i], statusitems[i]);
  }

  exitFunc ('test');
}
