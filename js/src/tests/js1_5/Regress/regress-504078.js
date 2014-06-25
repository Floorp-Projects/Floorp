/* -*- tab-width: 2; indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 504078;
var summary = 'Iterations over global object';
var actual = '';
var expect = '';

var g = (typeof window == 'undefined' ? this : window);

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  function keys(obj) {
    for (var prop in obj) {
    }
  }

  var data = { a : 1, b : 2 };
  var data2 = { a : 1, b : 2 };

  function boot() {
	  keys(data);
    keys(g);
    keys(data2); // this call dies with "var prop is not a function"
    print('no error');
  }

  try
  {
    boot();
  }
  catch(ex)
  {
    actual = ex + '';
  }

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
