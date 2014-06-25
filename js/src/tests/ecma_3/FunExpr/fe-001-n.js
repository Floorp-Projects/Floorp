/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


DESCRIPTION = "Previous statement should have thrown a ReferenceError";
EXPECTED = "error";

test();

function test()
{
  enterFunc ("test");
  printStatus ("Function Expression test.");

  var x = function f(){return "inner";}();
  var y = f();   
  reportCompare('PASS', 'FAIL', "Previous statement should have thrown a ReferenceError");

  exitFunc ("test");
}
