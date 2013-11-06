/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

module.metadata = {
  'engines': {
    'Firefox': '*'
  }
};

const { safeMerge: merge } = require('sdk/util/object');

merge(module.exports,
  require('./tests/test-places-bookmarks'),
  require('./tests/test-places-events'),
  require('./tests/test-places-favicon'),
  require('./tests/test-places-history'),
  require('./tests/test-places-host'),
  require('./tests/test-places-utils')
);

require('sdk/test/runner').runTestsFromModule(module);
