/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 344959;
var summary = 'Functions should not lose scope chain after exception';
var actual = '';
var expect = 'with';


//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
 
  var x = "global"

    with ({x:"with"})
    actual = (function() { try {} catch(exc) {}; return x }());

  reportCompare(expect, actual, summary + ': 1');

  with ({x:"with"})
    actual = (function() { try { throw 1} catch(exc) {}; return x }());

  reportCompare(expect, actual, summary + ': 2');

  exitFunc ('test');
}
