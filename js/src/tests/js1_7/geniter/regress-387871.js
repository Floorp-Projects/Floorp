/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER = 387871;
var summary = 'Do not assert: gen->state != JSGEN_RUNNING && gen->state != JSGEN_CLOSING';
var actual = 'No Crash';
var expect = 'No Crash';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var a = gen();

  try {
    a.next();
    throw "a.next() should throw about already invoked generator";
  } catch (e) {
    if (!(e instanceof TypeError))
      throw e;
  }

  function gen()
  {
    for (x in a)
      yield 1;
  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
