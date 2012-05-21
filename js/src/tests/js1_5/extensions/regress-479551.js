/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  if (typeof shapeOf != 'function')
  {
    print(expect = actual = 'Test skipped: requires shell');
  }
  else
  {
    jit(true);

    var o = {a:3, b:2};
    shapeOf(o);
    var p = {};
    p.a =3;
    p.b=2;
    shapeOf(p);

    jit(false);

  }
  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
