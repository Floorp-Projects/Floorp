/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { merge } = require('sdk/util/object');

merge(module.exports, require('./test-tab'));
merge(module.exports, require('./test-tab-events'));
merge(module.exports, require('./test-tab-observer'));
merge(module.exports, require('./test-tab-utils'));

require('sdk/test/runner').runTestsFromModule(module);
