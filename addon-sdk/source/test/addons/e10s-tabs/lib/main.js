/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { merge } = require('sdk/util/object');
const { version, platform } = require('sdk/system');
const { getMostRecentBrowserWindow, isBrowser } = require('sdk/window/utils');
const { WindowTracker } = require('sdk/deprecated/window-utils');
const { close, focus } = require('sdk/window/helpers');
const { when } = require('sdk/system/unload');

function replaceWindow(remote) {
  let next = null;
  let old = getMostRecentBrowserWindow();
  let promise = new Promise(resolve => {
    let tracker = WindowTracker({
      onTrack: window => {
        if (window !== next)
          return;
        resolve(window);
        tracker.unload();
      }
    });
  })
  next = old.OpenBrowserWindow({ remote });
  return promise.then(focus).then(_ => close(old));
}

// merge(module.exports, require('./test-tab'));
merge(module.exports, require('./test-tab-events'));
merge(module.exports, require('./test-tab-observer'));
merge(module.exports, require('./test-tab-utils'));

// run e10s tests only on builds from trunk, fx-team, Nightly..
if (!version.endsWith('a1')) {
  module.exports = {};
}

// bug 1054482 - e10s test addons time out on linux
if (platform === 'linux') {
  module.exports = {};
  require('sdk/test/runner').runTestsFromModule(module);
}
else {
  replaceWindow(true).then(_ =>
    require('sdk/test/runner').runTestsFromModule(module));

  when(_ => replaceWindow(false));
}
