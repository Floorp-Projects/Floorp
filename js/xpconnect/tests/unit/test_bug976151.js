/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cu = Components.utils;
function run_test() {
  let unprivilegedSb = new Cu.Sandbox('http://www.example.com');
  function checkCantWrap(val) {
    try {
      unprivilegedSb.prop = val;
      do_check_true(false);
    } catch (e) {
      do_check_true(/denied|insecure|/.test(e));
    }
  }
  let xoSb = new Cu.Sandbox('http://www.example.net');
  let epSb = new Cu.Sandbox(['http://www.example.com']);
  checkCantWrap(eval);
  checkCantWrap(xoSb.eval);
  checkCantWrap(epSb.eval);
  checkCantWrap(Function);
  checkCantWrap(xoSb.Function);
  checkCantWrap(epSb.Function);
}
