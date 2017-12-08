/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 466905;
var summary = 'Prototypes of sandboxed arrays';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  if (typeof evalcx != 'function')
  {
    expect = actual = 'Test skipped: requires evalcx support';
  }
  else
  {
    expect = true;

    function createArray()
    {
      var a;                
      for (var i = 0; i < 10; i++)
        a = [1, 2, 3, 4, 5];
      return a;
    }

    var sandbox = evalcx("lazy");
    sandbox.createArray = createArray;
    var p1 = Object.getPrototypeOf(createArray());
    var p2 = Object.getPrototypeOf(evalcx("createArray()", sandbox));
    print(actual = (p1 === p2));
  }

  reportCompare(expect, actual, summary);
}
