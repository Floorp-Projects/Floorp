/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 341956;
var summary = 'GC Hazards in jsarray.c - pop';
var actual = '';
var expect = 'GETTER RESULT';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var N = 0xFFFFFFFF;
  var a = []; 
  a[N - 1] = 0;

  var expected = "GETTER RESULT";

  a.__defineGetter__(N - 1, function() {
		       delete a[N - 1];
		       var tmp = [];
		       tmp[N - 2] = 1;

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
		       return expected;
		     });

  actual = a.pop();

  reportCompare(expect, actual, summary);

  print('Done');
}
