/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { id } = require('sdk/self');
const { getAddonByID } = require('sdk/addon/manager');

exports.testDevelopers = function*(assert) {
  let addon = yield getAddonByID(id);
  let count = 0;
  addon.developers.forEach(({ name }) => {
    assert.equal(name, ++count == 1 ? 'A' : 'B', 'The developers keys are correct');
  });
  assert.equal(count, 2, 'The key count is correct');
  assert.equal(addon.developers.length, 2, 'The key length is correct');
}

require('sdk/test/runner').runTestsFromModule(module);
