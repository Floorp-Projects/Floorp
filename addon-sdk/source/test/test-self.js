/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Cc, Ci, Cu, Cm, components } = require("chrome");
const xulApp = require("sdk/system/xul-app");
const self = require("sdk/self");

const { AddonManager } = Cu.import("resource://gre/modules/AddonManager.jsm", {});

exports.testSelf = function(assert) {
  var source = self.data.load("test-content-symbiont.js");
  assert.ok(source.match(/test-content-symbiont/), "self.data.load() works");

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

exports.testSelfID = function(assert, done) {
  var self = require("sdk/self");
  // We can't assert anything about the ID inside the unit test right now,
  // because the ID we get depends upon how the test was invoked. The idea
  // is that it is supposed to come from the main top-level package's
  // package.json file, from the "id" key.
  assert.equal(typeof(self.id), "string", "self.id is a string");
  assert.ok(self.id.length > 0);

  AddonManager.getAddonByID(self.id, function(addon) {
    if (!addon) {
      assert.fail("did not find addon with self.id");
    }
    else {
      assert.pass("found addon with self.id");
    }

    done();
  });
}

require("sdk/test").run(exports);
