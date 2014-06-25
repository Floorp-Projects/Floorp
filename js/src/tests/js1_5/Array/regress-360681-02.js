/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 360681;
var summary = 'Regression from bug 224128';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  expect = actual = 'No Crash';

  var N = 1000;

// Make an array with a hole at the end
  var a = Array(N);
  for (i = 0; i < N - 1; ++i)
    a[i] = 1;

// array_sort due for array with N elements with allocates a temporary vector
// with 2*N. Lets create strings that on 32 and 64 bit CPU cause allocation
// of the same amount of memory + 1 word for their char arrays. After we GC
// strings with a reasonable malloc implementation that memory will be most
// likely reused in array_sort for the temporary vector. Then the bug causes
// accessing the one-beyond-the-aloocation word and re-interpretation of
// 0xFFF0FFF0 as GC thing.

  var str1 = Array(2*(2*N + 1) + 1).join(String.fromCharCode(0xFFF0));
  var str2 = Array(4*(2*N + 1) + 1).join(String.fromCharCode(0xFFF0));
  gc();
  str1 = str2 = null;
  gc();

  var firstCall = true;
  a.sort(function (a, b) {
	   if (firstCall) {
	     firstCall = false;
	     gc();
	   }
	   return a - b;
	 });

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
