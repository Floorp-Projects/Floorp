/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { id } = require('sdk/self');
const { getAddonByID } = require('sdk/addon/manager');

exports.testContributors = function*(assert) {
  let addon = yield getAddonByID(id);
  assert.equal(addon.creator.name, 'test <test@mozilla.com>', '< and > characters work');
}

require('sdk/test/runner').runTestsFromModule(module);
