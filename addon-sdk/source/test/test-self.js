/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu, Cm, components} = require('chrome');
Cu.import("resource://gre/modules/AddonManager.jsm", this);

exports.testSelf = function(test) {
  var self = require("sdk/self");

  var source = self.data.load("test-content-symbiont.js");
  test.assert(source.match(/test-content-symbiont/), "self.data.load() works");

  // Likewise, we can't assert anything about the full URL, because that
  // depends on self.id . We can only assert that it ends in the right
  // thing.
  var url = self.data.url("test-content-symbiont.js");
  test.assertEqual(typeof(url), "string", "self.data.url('x') returns string");
  test.assertEqual(/\/test-content-symbiont\.js$/.test(url), true);

  // Make sure 'undefined' is not in url when no string is provided.
  url = self.data.url();
  test.assertEqual(typeof(url), "string", "self.data.url() returns string");
  test.assertEqual(/\/undefined$/.test(url), false);

  // When tests are run on just the api-utils package, self.name is
  // api-utils. When they're run as 'cfx testall', self.name is testpkgs.
  test.assert(self.name == "addon-sdk", "self.name is addon-sdk");

  // loadReason may change here, as we change the way tests addons are installed
  test.assertEqual(self.loadReason, "startup",
                   "self.loadReason is always `startup` on test runs");

  test.assertEqual(self.isPrivateBrowsingSupported, false,
                   'usePrivateBrowsing property is false by default');
};

exports.testSelfID = function(test) {
  test.waitUntilDone();

  var self = require("sdk/self");
  // We can't assert anything about the ID inside the unit test right now,
  // because the ID we get depends upon how the test was invoked. The idea
  // is that it is supposed to come from the main top-level package's
  // package.json file, from the "id" key.
  test.assertEqual(typeof(self.id), "string", "self.id is a string");
  test.assert(self.id.length > 0);

  AddonManager.getAddonByID(self.id, function(addon) {
    if (!addon) {
      test.fail("did not find addon with self.id");
    }
    else {
      test.pass("found addon with self.id");
    }
    test.done();
  });
}
