/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { getMostRecentBrowserWindow, isBrowser } = require('sdk/window/utils');
const { promise: windowPromise, close, focus } = require('sdk/window/helpers');
const { openTab, closeTab, getBrowserForTab } = require('sdk/tabs/utils');
const { WindowTracker } = require('sdk/deprecated/window-utils');
const { version, platform } = require('sdk/system');
const { when } = require('sdk/system/unload');
const tabs = require('sdk/tabs');

const SKIPPING_TESTS = {
  "test skip": (assert) => assert.pass("nothing to test here")
};

exports.testTabIsRemote = function(assert, done) {
  const url = 'data:text/html,test-tab-is-remote';
  let tab = openTab(getMostRecentBrowserWindow(), url);
  assert.ok(tab.linkedBrowser.isRemoteBrowser, "The new tab should be remote");

  // can't simply close a remote tab before it is loaded, bug 1006043
  let mm = getBrowserForTab(tab).messageManager;
  mm.addMessageListener('7', function listener() {
    mm.removeMessageListener('7', listener);
    tabs.once('close', done);
    closeTab(tab);
  })
  mm.loadFrameScript('data:,sendAsyncMessage("7")', true);
}

// run e10s tests only on builds from trunk, fx-team, Nightly..
if (!version.endsWith('a1')) {
  module.exports = {};
}

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

// bug 1054482 - e10s test addons time out on linux
if (platform === 'linux') {
  module.exports = SKIPPING_TESTS;
  require('sdk/test/runner').runTestsFromModule(module);
}
else {
  replaceWindow(true).then(_ =>
    require('sdk/test/runner').runTestsFromModule(module));

  when(_ => replaceWindow(false));
}
