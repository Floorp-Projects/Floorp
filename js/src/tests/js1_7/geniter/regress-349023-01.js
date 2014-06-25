/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 349023;
var summary = 'Bogus JSCLASS_IS_EXTENDED in the generator class';
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

  function gen() {
    var i = 0;
    yield i;
  }

  try
  {
    var g = gen();
    for (var i = 0; i < 10; i++) {
      print(g.next());
    }
  }
  catch(ex)
  {
  }
 
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
