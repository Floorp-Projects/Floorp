/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cu } = require('chrome');
const self = require('sdk/self');
const { AddonManager } = Cu.import('resource://gre/modules/AddonManager.jsm', {});

exports.testContributors = function(assert, done) {
  AddonManager.getAddonByID(self.id, (addon) => {
    assert.equal(addon.creator.name, 'test <test@mozilla.com>', '< and > characters work');
    done();
  });
}

require('sdk/test/runner').runTestsFromModule(module);
