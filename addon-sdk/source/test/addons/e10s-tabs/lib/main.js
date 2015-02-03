/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { merge } = require('sdk/util/object');
const { version } = require('sdk/system');

const SKIPPING_TESTS = {
  "test skip": (assert) => assert.pass("nothing to test here")
};

merge(module.exports, require('./test-tab'));
merge(module.exports, require('./test-tab-events'));
merge(module.exports, require('./test-tab-observer'));
merge(module.exports, require('./test-tab-utils'));

// run e10s tests only on builds from trunk, fx-team, Nightly..
if (!version.endsWith('a1')) {
  module.exports = SKIPPING_TESTS;
}

require('sdk/test/runner').runTestsFromModule(module);
