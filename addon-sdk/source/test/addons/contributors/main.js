/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Cu } = require('chrome');
const self = require('sdk/self');
const { AddonManager } = Cu.import('resource://gre/modules/AddonManager.jsm', {});

exports.testContributors = function(assert, done) {
  AddonManager.getAddonByID(self.id, function(addon) {
    let count = 0;
    addon.contributors.forEach(function({ name }) {
      count++;
      assert.equal(name, count == 1 ? 'A' : 'B', 'The contributors keys are correct');
    });
    assert.equal(count, 2, 'The key count is correct');
    assert.equal(addon.contributors.length, 2, 'The key length is correct');
    done();
  });
}

require('sdk/test/runner').runTestsFromModule(module);
