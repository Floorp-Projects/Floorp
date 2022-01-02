/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  let sb1 = new Cu.Sandbox("https://example.org");
  let throwingFunc = Cu.evalInSandbox("new Function('throw new Error')", sb1);
  // NOTE: Different origin from the other sandbox.
  let sb2 = new Cu.Sandbox("https://example.com");
  Cu.exportFunction(function() {
    // Call a different-compartment throwing function.
    throwingFunc();
  }, sb2, { defineAs: "func" });
  let threw = Cu.evalInSandbox("var threw; try { func(); threw = false; } catch (e) { threw = true } threw",
                                sb2);
  Assert.ok(threw);
}
