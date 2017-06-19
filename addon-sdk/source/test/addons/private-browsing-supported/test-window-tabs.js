/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const tabs = require('sdk/tabs');
const { isPrivate } = require('sdk/private-browsing');
const { promise: windowPromise, close, focus } = require('sdk/window/helpers');
const { getMostRecentBrowserWindow } = require('sdk/window/utils');

exports.testOpenTabWithPrivateActiveWindowNoIsPrivateOption = function(assert, done) {
  let window = getMostRecentBrowserWindow().OpenBrowserWindow({ private: true });

  windowPromise(window, 'load').then(focus).then(function (window) {
    assert.ok(isPrivate(window), 'new window is private');

    tabs.open({
      url: 'about:blank',
      onOpen: function(tab) {
        assert.ok(isPrivate(tab), 'new tab is private');
        close(window).then(done).catch(assert.fail);
      }
    })
  }).catch(assert.fail);
}

exports.testOpenTabWithNonPrivateActiveWindowNoIsPrivateOption = function(assert, done) {
  let window = getMostRecentBrowserWindow().OpenBrowserWindow({ private: false });

  windowPromise(window, 'load').then(focus).then(function (window) {
    assert.equal(isPrivate(window), false, 'new window is not private');

    tabs.open({
      url: 'about:blank',
      onOpen: function(tab) {
        assert.equal(isPrivate(tab), false, 'new tab is not private');
        close(window).then(done).catch(assert.fail);
      }
    })
  }).catch(assert.fail);
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
        close(window).then(done).catch(assert.fail);
      }
    })
  }).catch(assert.fail);
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
        close(window).then(done).catch(assert.fail);
      }
    })
  }).catch(assert.fail);
}
