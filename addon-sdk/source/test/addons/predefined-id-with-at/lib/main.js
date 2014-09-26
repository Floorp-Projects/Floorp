/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { id, preferencesBranch } = require('sdk/self');
const simple = require('sdk/simple-prefs');
const service = require('sdk/preferences/service');
const { getAddonByID } = require('sdk/addon/manager');

const expected_id = 'predefined-id@test';

exports.testExpectedID = function(assert) {
  assert.equal(id, expected_id, 'ID is as expected');
  assert.equal(preferencesBranch, expected_id, 'preferences-branch is ' + expected_id);

  assert.equal(simple.prefs.test, 5, 'test pref is 5');

  simple.prefs.test2 = '25';
  assert.equal(service.get('extensions.'+expected_id+'.test2'), '25', 'test pref is 25');
  assert.equal(service.get('extensions.'+expected_id+'.test2'), simple.prefs.test2, 'test pref is 25');
}

exports.testSelfID = function*(assert) {
  assert.equal(typeof(id), 'string', 'self.id is a string');
  assert.ok(id.length > 0, 'self.id not empty');

  let addon = yield getAddonByID(id);
  assert.equal(addon.id, id, 'found addon with self.id');
}

require('sdk/test/runner').runTestsFromModule(module);
