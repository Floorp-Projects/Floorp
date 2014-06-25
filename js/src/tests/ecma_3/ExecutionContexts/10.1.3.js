/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


/**
   ECMA Section: 10.1.3: Variable Instantiation
   FunctionDeclarations are processed before VariableDeclarations, and
   VariableDeclarations don't replace existing values with undefined
*/

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
  enterFunc ("test");

  printStatus ("ECMA Section: 10.1.3: Variable Instantiation.");
  printBugNumber (17290);

  reportCompare ("function", f(), "Declaration precedence test");

  exitFunc("test");       
}
