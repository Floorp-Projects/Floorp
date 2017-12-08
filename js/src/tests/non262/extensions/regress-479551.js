/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 479551;
var summary = 'Do not assert: (cx)->requestDepth || (cx)->thread == (cx)->runtime->gcThread';
var actual = '';
var expect = '';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  if (typeof shapeOf != 'function')
  {
    print(expect = actual = 'Test skipped: requires shell');
  }
  else
  {

    var o = {a:3, b:2};
    shapeOf(o);
    var p = {};
    p.a =3;
    p.b=2;
    shapeOf(p);


  }
  reportCompare(expect, actual, summary);
}
