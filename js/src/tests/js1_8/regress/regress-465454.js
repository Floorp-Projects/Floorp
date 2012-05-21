/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 465454;
var summary = 'TM: do not crash with type-unstable loop';
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

  jit(true);

  for each (let b in [(-1/0), new String(''), new String(''), null, (-1/0),
                      (-1/0), new String(''), new String(''), null]) '' + b;

  jit(false);

  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
