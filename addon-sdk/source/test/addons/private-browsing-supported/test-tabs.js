'use strict';

const tabs = require('sdk/tabs');
const { is } = require('sdk/system/xul-app');
const { isPrivate } = require('sdk/private-browsing');
const pbUtils = require('sdk/private-browsing/utils');
const { getOwnerWindow } = require('sdk/private-browsing/window/utils');
const { promise: windowPromise, close, focus } = require('sdk/window/helpers');
const { getMostRecentBrowserWindow } = require('sdk/window/utils');

exports.testPrivateTabsAreListed = function (assert, done) {
  let originalTabCount = tabs.length;

  tabs.open({
    url: 'about:blank',
    isPrivate: true,
    onOpen: function(tab) {
      let win = getOwnerWindow(tab);
      // PWPB case
      if (pbUtils.isWindowPBSupported || pbUtils.isTabPBSupported) {
        assert.ok(isPrivate(tab), "tab is private");
        assert.equal(tabs.length, originalTabCount + 1,
                     'New private window\'s tab are visible in tabs list');
      }
      else {
      // Global case, openDialog didn't opened a private window/tab
        assert.ok(!isPrivate(tab), "tab isn't private");
        assert.equal(tabs.length, originalTabCount + 1,
                     'New non-private window\'s tab is visible in tabs list');
      }

      tab.close(done);
    }
  });
}

exports.testOpenTabWithPrivateActiveWindowNoIsPrivateOption = function(assert, done) {
  let window = getMostRecentBrowserWindow().OpenBrowserWindow({ private: true });

  windowPromise(window, 'load').then(focus).then(function (window) {
    assert.ok(isPrivate(window), 'new window is private');

    tabs.open({
      url: 'about:blank',
      onOpen: function(tab) {
        assert.ok(isPrivate(tab), 'new tab is private');
        assert.ok(isPrivate(getOwnerWindow(tab)), 'new tab window is private');
        assert.strictEqual(getOwnerWindow(tab), window, 'the tab window and the private window are the same');

        close(window).then(done, assert.fail);
      }
    })
  }, assert.fail).then(null, assert.fail);
}

exports.testOpenTabWithNonPrivateActiveWindowNoIsPrivateOption = function(assert, done) {
  let window = getMostRecentBrowserWindow().OpenBrowserWindow({ private: false });

  windowPromise(window, 'load').then(focus).then(function (window) {
    assert.equal(isPrivate(window), false, 'new window is not private');

    tabs.open({
      url: 'about:blank',
      onOpen: function(tab) {
        assert.equal(isPrivate(tab), false, 'new tab is not private');
        assert.equal(isPrivate(getOwnerWindow(tab)), false, 'new tab window is not private');
        assert.strictEqual(getOwnerWindow(tab), window, 'the tab window and the new window are the same');

        close(window).then(done, assert.fail);
      }
    })
  }, assert.fail).then(null, assert.fail);
}

exports.testOpenTabWithPrivateActiveWindowWithIsPrivateOptionTrue = function(assert, done) {
  let window = getMostRecentBrowserWindow().OpenBrowserWindow({ private: true });

  windowPromise(window, 'load').then(focus).then(function (window) {
    assert.ok(isPrivate(window), 'new window is private');

    tabs.open({
      url: 'about:blank',
      isPrivate: true,
      onOpen: function(tab) {
        assert.ok(isPrivate(tab), 'new tab is private');
        assert.ok(isPrivate(getOwnerWindow(tab)), 'new tab window is private');
        assert.strictEqual(getOwnerWindow(tab), window, 'the tab window and the private window are the same');

        close(window).then(done, assert.fail);
      }
    })
  }, assert.fail).then(null, assert.fail);
}

exports.testOpenTabWithNonPrivateActiveWindowWithIsPrivateOptionFalse = function(assert, done) {
  let window = getMostRecentBrowserWindow().OpenBrowserWindow({ private: false });

  windowPromise(window, 'load').then(focus).then(function (window) {
    assert.equal(isPrivate(window), false, 'new window is not private');

    tabs.open({
      url: 'about:blank',
      isPrivate: false,
      onOpen: function(tab) {
        assert.equal(isPrivate(tab), false, 'new tab is not private');
        assert.equal(isPrivate(getOwnerWindow(tab)), false, 'new tab window is not private');
        assert.strictEqual(getOwnerWindow(tab), window, 'the tab window and the new window are the same');

        close(window).then(done, assert.fail);
      }
    })
  }, assert.fail).then(null, assert.fail);
}
