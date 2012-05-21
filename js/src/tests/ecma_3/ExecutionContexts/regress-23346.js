/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var CALL_CALLED = "PASSED";

test();

function f(x)
{
  if (x)
    return call();

  return "FAILED!";
}

function call()
{
  return CALL_CALLED;
}

function test()
{
  enterFunc ("test");

  printStatus ("ECMA Section: 10.1.3: Variable Instantiation.");
  printBugNumber (23346);

  reportCompare ("PASSED", f(true),
		 "Unqualified reference should not see Function.prototype");

  exitFunc("test");       
}
