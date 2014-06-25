/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 354499;
var summary = 'Iterating over Array elements';
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

  function get_value()
  {
    // Unroot the first element
    this[0] = 0;

    // Call gc to collect atom corresponding to Math.sqrt(2)
    gc();
  }

  var counter = 2;
  Iterator.prototype.next = function()
    {
      if (counter-- <= 0) throw StopIteration;
      var a = [Math.sqrt(2), 1];
      a.__defineGetter__(1, get_value);
      return a;
    };

  for (i in [1])
    ;

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
