/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   ECMA Section: 10.1.4.1 Entering An Execution Context
   ECMA says:
   * Global Code, Function Code
   Variable instantiation is performed using the global object as the
   variable object and using property attributes { DontDelete }.

   * Eval Code
   Variable instantiation is performed using the calling context's
   variable object and using empty property attributes.
*/

var BUGNUMBER = '(none)';
var summary = '10.1.4.1 Entering An Execution Context';
var actual = '';
var expect = '';

test();

function test()
{
  printBugNumber(BUGNUMBER);
  printStatus (summary);

  var y;
  eval("var x = 1");

  if (delete y)
    reportCompare('PASS', 'FAIL', "Expected *NOT* to be able to delete y");

  if (typeof x == "undefined")
    reportCompare('PASS', 'FAIL', "x did not remain defined after eval()");
  else if (x != 1)
    reportCompare('PASS', 'FAIL', "x did not retain it's value after eval()");
   
  if (!delete x)
    reportCompare('PASS', 'FAIL', "Expected to be able to delete x");

  reportCompare('PASS', 'PASS', '10.1.4.1 Entering An Execution Context');
}
