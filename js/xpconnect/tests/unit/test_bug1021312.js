/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
function run_test() {
  let sb = new Cu.Sandbox(this);
  var called = false;

  Cu.exportFunction(function(str) { do_check_true(/someString/.test(str)); called = true; },
                    sb, { defineAs: "func" });
  // Do something weird with the string to make sure that it doesn't get interned.
  Cu.evalInSandbox("var str = 'someString'; for (var i = 0; i < 10; ++i) str += i;", sb);
  Cu.evalInSandbox("func(str);", sb);
  do_check_true(called);
}
