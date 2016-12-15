/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;

var srvScope = {};

function success(result) {
  ok(false, "script should not have loaded successfully");
  do_test_finished();
}

function error(err) {
  ok(err instanceof SyntaxError, "loading script with early error asynchronously resulted in error");
  do_test_finished();
}

function run_test() {
  do_test_pending();

  var file = do_get_file("subScriptWithEarlyError.js");
  var ios = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);
  var uri = ios.newFileURI(file);
  var scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                       .getService(Ci.mozIJSSubScriptLoader);
  var p = scriptLoader.loadSubScriptWithOptions(uri.spec,
                                                { target: srvScope,
                                                  async: true });
  p.then(success, error);
}
