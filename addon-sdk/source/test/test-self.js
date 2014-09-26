/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const xulApp = require("sdk/system/xul-app");
const self = require("sdk/self");
const { Loader, main, unload } = require("toolkit/loader");
const loaderOptions = require("@loader/options");

exports.testSelf = function(assert) {
  // Likewise, we can't assert anything about the full URL, because that
  // depends on self.id . We can only assert that it ends in the right
  // thing.
  var url = self.data.url("test-content-symbiont.js");
  assert.equal(typeof(url), "string", "self.data.url('x') returns string");
  assert.equal(/\/test-content-symbiont\.js$/.test(url), true);

  // Make sure 'undefined' is not in url when no string is provided.
  url = self.data.url();
  assert.equal(typeof(url), "string", "self.data.url() returns string");
  assert.equal(/\/undefined$/.test(url), false);

  // When tests are run on just the api-utils package, self.name is
  // api-utils. When they're run as 'cfx testall', self.name is testpkgs.
  assert.equal(self.name, "addon-sdk", "self.name is addon-sdk");

  // loadReason may change here, as we change the way tests addons are installed
  // Bug 854937 fixed loadReason and is now install
  let testLoadReason = xulApp.versionInRange(xulApp.platformVersion,
                                             "23.0a1", "*") ? "install"
                                                            : "startup";
  assert.equal(self.loadReason, testLoadReason,
               "self.loadReason is either startup or install on test runs");

  assert.equal(self.isPrivateBrowsingSupported, false,
               'usePrivateBrowsing property is false by default');
};

exports.testSelfHandlesLackingLoaderOptions = function (assert) {
  let root = module.uri.substr(0, module.uri.lastIndexOf('/'));
  let uri = root + '/fixtures/loader/self/';
  let sdkPath = loaderOptions.paths[''] + 'sdk';
  let loader = Loader({ paths: { '': uri, 'sdk': sdkPath }});
  let program = main(loader, 'main');
  let self = program.self;
  assert.pass("No errors thrown when including sdk/self without loader options");
  assert.equal(self.isPrivateBrowsingSupported, false,
    "safely checks sdk/self.isPrivateBrowsingSupported");
  assert.equal(self.packed, false,
    "safely checks sdk/self.packed");
  unload(loader);
};

require("sdk/test").run(exports);
