/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { merge } = require('sdk/util/object');
const { get } = require('sdk/preferences/service');

merge(module.exports, require('./test-tab'));
merge(module.exports, require('./test-tab-events'));
merge(module.exports, require('./test-tab-observer'));
merge(module.exports, require('./test-tab-utils'));

// e10s tests should not ride the train to aurora
if (get('app.update.channel') !== 'nightly') {
  module.exports = {};
}

require('sdk/test/runner').runTestsFromModule(module);
