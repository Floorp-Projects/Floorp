/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  var file = do_get_file("bug451678_subscript.js");
  var ios = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);
  var uri = ios.newFileURI(file);
  var scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                       .getService(Ci.mozIJSSubScriptLoader);
  var srvScope = {};
  scriptLoader.loadSubScript(uri.spec, srvScope);
  Assert.ok('makeTags' in srvScope && srvScope.makeTags instanceof Function);
}
