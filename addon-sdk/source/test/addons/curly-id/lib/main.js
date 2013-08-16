/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { id } = require('sdk/self');
const simple = require('sdk/simple-prefs');
const service = require('sdk/preferences/service');
const { preferencesBranch } = require('@loader/options');
const { AddonManager } = require('chrome').Cu.import('resource://gre/modules/AddonManager.jsm');

exports.testCurlyID = function(assert) {
  assert.equal(id, '{34a1eae1-c20a-464f-9b0e-000000000000}', 'curly ID is curly');

  assert.equal(simple.prefs.test13, 26, 'test13 is 26');

  simple.prefs.test14 = '15';
  assert.equal(service.get('extensions.{34a1eae1-c20a-464f-9b0e-000000000000}.test14'), '15', 'test14 is 15');

  assert.equal(service.get('extensions.{34a1eae1-c20a-464f-9b0e-000000000000}.test14'), simple.prefs.test14, 'simple test14 also 15');
  
}

exports.testInvalidPreferencesBranch = function(assert) {
  assert.notEqual(preferencesBranch, 'invalid^branch*name', 'invalid preferences-branch value ignored');

  assert.equal(preferencesBranch, '{34a1eae1-c20a-464f-9b0e-000000000000}', 'preferences-branch is {34a1eae1-c20a-464f-9b0e-000000000000}');

}

// from `/test/test-self.js`, adapted to `sdk/test/assert` API
exports.testSelfID = function(assert, done) {

  assert.equal(typeof(id), 'string', 'self.id is a string');
  assert.ok(id.length > 0, 'self.id not empty');

  AddonManager.getAddonByID(id, function(addon) {
    assert.ok(addon, 'found addon with self.id');
    done();
  });

}

require('sdk/test/runner').runTestsFromModule(module);
