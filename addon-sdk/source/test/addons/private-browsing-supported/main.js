/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { merge } = require('sdk/util/object');
const app = require('sdk/system/xul-app');

merge(module.exports,
  require('./test-tabs'),
  require('./test-page-mod'),
  require('./test-private-browsing'),
  require('./test-sidebar')
);

// Doesn't make sense to test window-utils and windows on fennec,
// as there is only one window which is never private. Also ignore
// unsupported modules (panel, selection)
if (!app.is('Fennec')) {
  merge(module.exports,
    require('./test-selection'),
    require('./test-panel'),
    require('./test-window-tabs'),
    require('./test-windows')
  );
}

require('sdk/test/runner').runTestsFromModule(module);
