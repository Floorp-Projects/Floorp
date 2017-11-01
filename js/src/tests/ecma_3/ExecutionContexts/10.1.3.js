/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   ECMA Section: 10.1.3: Variable Instantiation
   FunctionDeclarations are processed before VariableDeclarations, and
   VariableDeclarations don't replace existing values with undefined
*/

var BUGNUMBER = 17290;

test();

function f()
{
  var x;

  return typeof x;

  function x()
  {
    return 7;   
  }
}

function test()
{
  printStatus ("ECMA Section: 10.1.3: Variable Instantiation.");
  printBugNumber (BUGNUMBER);

  reportCompare ("function", f(), "Declaration precedence test");
}
