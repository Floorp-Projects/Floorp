/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 457065;
var summary = 'Do not assert: !fp->callee || fp->thisp == fp->argv[-1].toObjectOrNull()';
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


  (function() {
    new function (){ for (var x = 0; x < 3; ++x){} };
  })();


  reportCompare(expect, actual, summary);

  exitFunc ('test');
}
