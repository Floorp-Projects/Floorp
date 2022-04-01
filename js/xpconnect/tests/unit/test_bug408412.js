/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
function run_test() {
  try {
    ChromeUtils.import("resource://test/syntax_error.jsm");
    do_throw("Failed to report any error at all");
  } catch (e) {
    Assert.notEqual(/^SyntaxError:/.exec(e + ''), null);
  }
}
