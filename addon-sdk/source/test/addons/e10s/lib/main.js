/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { get: getPref } = require('sdk/preferences/service');
const { getMostRecentBrowserWindow } = require('sdk/window/utils');
const { openTab, closeTab, getBrowserForTab } = require('sdk/tabs/utils');

exports.testRemotePrefIsSet = function(assert) {
  assert.ok(getPref('browser.tabs.remote.autostart'), 
            "Electrolysis remote tabs pref should be set");
}

exports.testTabIsRemote = function(assert, done) {
  const url = 'data:text/html,test-tab-is-remote';
  let tab = openTab(getMostRecentBrowserWindow(), url);
  assert.ok(tab.getAttribute('remote'), "The new tab should be remote");

  // can't simply close a remote tab before it is loaded, bug 1006043
  let mm = getBrowserForTab(tab).messageManager;
  mm.addMessageListener(7, function() {
    closeTab(tab);
    done();
  })
  mm.loadFrameScript('data:,sendAsyncMessage(7)', false);
}

require('sdk/test/runner').runTestsFromModule(module);
