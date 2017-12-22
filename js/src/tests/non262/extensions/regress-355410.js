/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 355410;
var summary = 'GC hazard in for([k,v] in o){...}';
var actual = 'No Crash';
var expect = 'No Crash';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var address = 0xbadf00d0, basket = { food: {} };
  var AP = Array.prototype, rooter = {};
  AP.__defineGetter__(0, function() { return this[-1]; });
  AP.__defineSetter__(0, function(v) {
			basket.food = null;
			for(var i = 0; i < 8 * 1024; i++) {
			  rooter[i] = 0x10000000000000 + address; // IEEE754!
			}
			return this[-1] = v;
		      });
  for(var [key, value] in basket) { value.trigger; }

  delete Array.prototype[0];

  reportCompare(expect, actual, summary);
}
