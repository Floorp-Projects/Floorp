/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
function run_test() {
  var file = do_get_file("syntax_error.jsm");
  var ios = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);
  var uri = ios.newFileURI(file);

  try {
    ChromeUtils.import(uri.spec);
    do_throw("Failed to report any error at all");
  } catch (e) {
    Assert.notEqual(/^SyntaxError:/.exec(e + ''), null);
  }
}
