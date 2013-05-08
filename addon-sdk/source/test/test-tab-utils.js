'use strict';

const { getTabs } = require('sdk/tabs/utils');
const { isGlobalPBSupported, isWindowPBSupported, isTabPBSupported } = require('sdk/private-browsing/utils');
const { browserWindows } = require('sdk/windows');
const tabs = require('sdk/tabs');
const { pb } = require('./private-browsing/helper');
const { isPrivate } = require('sdk/private-browsing');
const { openTab, closeTab, getTabContentWindow } = require('sdk/tabs/utils');
const { open, close } = require('sdk/window/helpers');
const { windows } = require('sdk/window/utils');
const { getMostRecentBrowserWindow } = require('sdk/window/utils');
const { fromIterator } = require('sdk/util/array');

if (isGlobalPBSupported) {
  exports.testGetTabs = function(assert, done) {
    pb.once('start', function() {
      tabs.open({
        url: 'about:blank',
        inNewWindow: true,
        onOpen: function(tab) {
          assert.equal(getTabs().length, 2, 'there are two tabs');
          assert.equal(browserWindows.length, 2, 'there are two windows');
          pb.once('stop', function() {
            done();
          });
          pb.deactivate();
        }
      });
    });
    pb.activate();
  };
}
else if (isWindowPBSupported) {
  exports.testGetTabs = function(assert, done) {
    open(null, {
        features: {
        private: true,
        toolbar: true,
        chrome: true
      }
    }).then(function(window) {
      assert.ok(isPrivate(window), 'new tab is private');
      assert.equal(getTabs().length, 1, 'there is one tab found');
      assert.equal(browserWindows.length, 1, 'there is one window found');
      fromIterator(browserWindows).forEach(function(window) {
        assert.ok(!isPrivate(window), 'all found windows are not private');
      });
      assert.equal(windows(null, {includePrivate: true}).length, 2, 'there are really two windows');
      close(window).then(done);
    });
  };
}
else if (isTabPBSupported) {
  exports.testGetTabs = function(assert, done) {
    let startTabCount = getTabs().length;
    let tab = openTab(getMostRecentBrowserWindow(), 'about:blank', {
      isPrivate: true
    });

    assert.ok(isPrivate(getTabContentWindow(tab)), 'new tab is private');
    let utils_tabs = getTabs();
    assert.equal(utils_tabs.length, startTabCount + 1,
                 'there are two tabs found');
    assert.equal(utils_tabs[utils_tabs.length-1], tab,
                 'the last tab is the opened tab');
    assert.equal(browserWindows.length, 1, 'there is only one window');
    closeTab(tab);

    done();
  };
}

// Test disabled because of bug 855771
module.exports = {};

require('test').run(exports);
