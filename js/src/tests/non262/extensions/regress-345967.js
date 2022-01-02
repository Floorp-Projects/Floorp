// |reftest| slow
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 345967;
var summary = 'Yet another unrooted atom in jsarray.c';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  expectExitCode(0);
  expectExitCode(3);

  print('This test will probably run out of memory');
  print('This test really should only fail on 64 bit machines');
 
  var JSVAL_INT_MAX = (1 << 30) - 1;

  var a = new Array(JSVAL_INT_MAX + 2);
  a[JSVAL_INT_MAX] = 0;
  a[JSVAL_INT_MAX + 1] = 1;

  a.__defineGetter__(JSVAL_INT_MAX, function() { return 0; });

  a.__defineSetter__(JSVAL_INT_MAX, function(value) {
		       delete a[JSVAL_INT_MAX + 1];
		       var tmp = [];
		       tmp[JSVAL_INT_MAX + 2] = 2;

		       if (typeof gc == 'function')
			 gc();
		       for (var i = 0; i != 50000; ++i) {
			 var tmp = 1 / 3;
			 tmp /= 10;
		       }
		       for (var i = 0; i != 1000; ++i) {
			 // Make string with 11 characters that would take
			 // (11 + 1) * 2 bytes or sizeof(JSAtom) so eventually
			 // malloc will ovewrite just freed atoms.
			 var tmp2 = Array(12).join(' ');
		       }
		     });


  a.shift();

  expect = 0;
  actual = a[JSVAL_INT_MAX];
  if (expect !== actual)
    print("BAD");

  reportCompare(expect, actual, summary);
}
