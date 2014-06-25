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

  var obj = {get a(){ return new Object(); }};

  function setter(v)
  {
    // Push out obj.a from all temp roots
    var tmp = { get toString() { return new Object(); }};
    try { String(tmp); } catch (e) {  }
    gc();
  }

  Array.prototype.__defineGetter__(0, function() { });
  Array.prototype.__defineSetter__(0, setter);

  for (var i in Iterator(obj))
    print(uneval(i));

  delete Array.prototype[0];

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
