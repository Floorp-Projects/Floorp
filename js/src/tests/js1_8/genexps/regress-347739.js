/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 347739;
var summary = 'generator_instance.close readonly and immune';
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

  function gen_test(test_index)
  {
    try {
      yield 1;
    } finally {
      actual += "Inside finally: "+test_index + ' ';
    }
  }

  actual = '';
  expect = 'Inside finally: 1 Inside finally: 2 ';

  var iter1 = gen_test(1);
  var close = iter1.close;
  iter1.close = null;
  delete iter1.close;
  iter1.next();
  iter1.close();

  var iter2 = gen_test(2);
  for (i in iter2) {
    iter2.close = null;
    delete iter2.close;
  }

  reportCompare(expect, actual, summary + ': 2');

  exitFunc ('test');
}
