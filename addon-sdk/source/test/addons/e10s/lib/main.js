/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { get: getPref } = require('sdk/preferences/service');
const { getMostRecentBrowserWindow } = require('sdk/window/utils');
const { openTab, closeTab, getBrowserForTab } = require('sdk/tabs/utils');
const tabs = require('sdk/tabs');

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
  mm.addMessageListener('7', function listener() {
    mm.removeMessageListener('7', listener);
    tabs.once('close', done);
    closeTab(tab);
  })
  mm.loadFrameScript('data:,sendAsyncMessage("7")', true);
}

// e10s tests should not ride the train to aurora, beta
if (getPref('app.update.channel') !== 'nightly') {
  module.exports = {};
}

require('sdk/test/runner').runTestsFromModule(module);
