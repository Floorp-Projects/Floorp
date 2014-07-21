/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
function run_test() {
  let unprivilegedSb = new Cu.Sandbox('http://www.example.com');
  function checkOpaqueWrapper(val) {
    unprivilegedSb.prop = val;
    try {
      Cu.evalInSandbox('prop();', sb);
    } catch (e) {
      do_check_true(/denied|insecure|/.test(e));
    }
  }
  let xoSb = new Cu.Sandbox('http://www.example.net');
  let epSb = new Cu.Sandbox(['http://www.example.com']);
  checkOpaqueWrapper(eval);
  checkOpaqueWrapper(xoSb.eval);
  checkOpaqueWrapper(epSb.eval);
  checkOpaqueWrapper(Function);
  checkOpaqueWrapper(xoSb.Function);
  checkOpaqueWrapper(epSb.Function);
}
