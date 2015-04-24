/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const xulApp = require("sdk/system/xul-app");
const self = require("sdk/self");
const { Loader, main, unload, override } = require("toolkit/loader");
const { PlainTextConsole } = require("sdk/console/plain-text");
const { Loader: CustomLoader } = require("sdk/test/loader");
const loaderOptions = require("@loader/options");

exports.testSelf = function(assert) {
  // Likewise, we can't assert anything about the full URL, because that
  // depends on self.id . We can only assert that it ends in the right
  // thing.
  var url = self.data.url("test.html");
  assert.equal(typeof(url), "string", "self.data.url('x') returns string");
  assert.equal(/\/test\.html$/.test(url), true);

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

exports.testPreferencesBranch = function (assert) {
  let options = override(loaderOptions, {
    preferencesBranch: 'human-readable',
  });
  let loader = CustomLoader(module, { }, options);
  let { preferencesBranch } = loader.require('sdk/self');
  assert.equal(preferencesBranch, 'human-readable',
    'preferencesBranch is human-readable');
}

exports.testInvalidPreferencesBranch = function (assert) {
  let console = new PlainTextConsole(_ => void _);
  let options = override(loaderOptions, {
    preferencesBranch: 'invalid^branch*name',
    id: 'simple@jetpack'
  });
  let loader = CustomLoader(module, { console }, options);
  let { preferencesBranch } = loader.require('sdk/self');
  assert.equal(preferencesBranch, 'simple@jetpack',
    'invalid preferencesBranch value ignored');
}

require("sdk/test").run(exports);
