/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

const { safeMerge: merge } = require('sdk/util/object');
const app = require("sdk/system/xul-app");

// Once Bug 903018 is resolved, just move the application testing to
// module.metadata.engines
if (app.is('Firefox')) {
  merge(module.exports,
    require('./tests/test-places-events'),
    require('./tests/test-places-bookmarks'),
    require('./tests/test-places-favicon'),
    require('./tests/test-places-history'),
    require('./tests/test-places-host'),
    require('./tests/test-places-utils')
  );
} else {
  exports['test unsupported'] = (assert) => {
    assert.pass('This application is unsupported.');
  };
}

require('sdk/test/runner').runTestsFromModule(module);
