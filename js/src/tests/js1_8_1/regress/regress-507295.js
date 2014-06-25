/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
var BUGNUMBER = 507295;
var summary = 'TM: assert with using result of assignment to closure var'
var actual = '';
var expect = 'do not crash';


//-----------------------------------------------------------------------------
start_test();
jit(true);

(function () {
    var y;
    (eval("(function () {\
               for (var x = 0; x < 3; ++x) {\
               ''.replace(/a/, (y = 3))\
               }\
           });\
     "))()
})()

jit(false);
actual = 'do not crash'
finish_test();
//-----------------------------------------------------------------------------

function start_test()
{
  enterFunc ('test');
  printBugNumber(BUGNUMBER);
  printStatus (summary);
}

function finish_test()
{
  reportCompare(expect, actual, summary);
  exitFunc ('test');
}
