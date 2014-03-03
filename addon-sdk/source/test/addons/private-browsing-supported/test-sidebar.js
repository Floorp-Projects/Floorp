/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Loader } = require('sdk/test/loader');
const { show, hide } = require('sdk/ui/sidebar/actions');
const { isShowing } = require('sdk/ui/sidebar/utils');
const { getMostRecentBrowserWindow, isWindowPrivate } = require('sdk/window/utils');
const { open, close, focus, promise: windowPromise } = require('sdk/window/helpers');
const { setTimeout } = require('sdk/timers');
const { isPrivate } = require('sdk/private-browsing');
const { data } = require('sdk/self');
const { URL } = require('sdk/url');

const { BUILTIN_SIDEBAR_MENUITEMS, isSidebarShowing,
        getSidebarMenuitems, getExtraSidebarMenuitems, makeID, simulateCommand,
        simulateClick, isChecked } = require('./sidebar/utils');

exports.testSideBarIsInNewPrivateWindows = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testSideBarIsInNewPrivateWindows';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,'+testName
  });

  let startWindow = getMostRecentBrowserWindow();
  let ele = startWindow.document.getElementById(makeID(testName));
  assert.ok(ele, 'sidebar element was added');

  open(null, { features: { private: true } }).then(function(window) {
      let ele = window.document.getElementById(makeID(testName));
      assert.ok(isPrivate(window), 'the new window is private');
      assert.ok(!!ele, 'sidebar element was added');

      sidebar.destroy();
      assert.ok(!window.document.getElementById(makeID(testName)), 'sidebar id DNE');
      assert.ok(!startWindow.document.getElementById(makeID(testName)), 'sidebar id DNE');

      return close(window);
  }).then(done).then(null, assert.fail);
}

// Disabled in order to land other fixes, see bug 910647 for further details.
/*
exports.testSidebarIsOpenInNewPrivateWindow = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testSidebarIsOpenInNewPrivateWindow';
  let window = getMostRecentBrowserWindow();

  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,'+testName
  });

  assert.equal(isPrivate(window), false, 'the window is not private');

  sidebar.on('show', function() {
    assert.equal(isSidebarShowing(window), true, 'the sidebar is showing');
    assert.equal(isShowing(sidebar), true, 'the sidebar is showing');

    windowPromise(window.OpenBrowserWindow({private: true}), 'DOMContentLoaded').then(function(window2) {
      assert.equal(isPrivate(window2), true, 'the new window is private');

      let sidebarEle = window2.document.getElementById('sidebar');

      // wait for the sidebar to load something
      function onSBLoad() {
        sidebarEle.contentDocument.getElementById('web-panels-browser').addEventListener('load', function() {
          assert.equal(isSidebarShowing(window), true, 'the sidebar is showing in old window still');
          assert.equal(isSidebarShowing(window2), true, 'the sidebar is showing in the new private window');
          assert.equal(isShowing(sidebar), true, 'the sidebar is showing');

          sidebar.destroy();
          close(window2).then(done);
        }, true);
      }

      sidebarEle.addEventListener('load', onSBLoad, true);

      assert.pass('waiting for the sidebar to open...');
    }, assert.fail).then(null, assert.fail);
  });

  sidebar.show();
}
*/
// TEST: edge case where web panel is destroyed while loading
exports.testDestroyEdgeCaseBugWithPrivateWindow = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testDestroyEdgeCaseBug';
  let window = getMostRecentBrowserWindow();
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,'+testName
  });

  // NOTE: purposely not listening to show event b/c the event happens
  //       between now and then.
  sidebar.show();

  assert.equal(isPrivate(window), false, 'the new window is not private');
  assert.equal(isSidebarShowing(window), true, 'the sidebar is showing');

  //assert.equal(isShowing(sidebar), true, 'the sidebar is showing');

  open(null, { features: { private: true } }).then(focus).then(function(window2) {
    assert.equal(isPrivate(window2), true, 'the new window is private');
    assert.equal(isSidebarShowing(window2), false, 'the sidebar is not showing');
    assert.equal(isShowing(sidebar), false, 'the sidebar is not showing');

    sidebar.destroy();
    assert.pass('destroying the sidebar');

    close(window2).then(function() {
      let loader = Loader(module);

      assert.equal(isPrivate(window), false, 'the current window is not private');

      let sidebar = loader.require('sdk/ui/sidebar').Sidebar({
        id: testName,
        title: testName,
        url:  'data:text/html;charset=utf-8,'+ testName,
        onShow: function() {
          assert.pass('onShow works for Sidebar');
          loader.unload();

          let sidebarMI = getSidebarMenuitems();
          for (let mi of sidebarMI) {
            assert.ok(BUILTIN_SIDEBAR_MENUITEMS.indexOf(mi.getAttribute('id')) >= 0, 'the menuitem is for a built-in sidebar')
            assert.ok(!isChecked(mi), 'no sidebar menuitem is checked');
          }
          assert.ok(!window.document.getElementById(makeID(testName)), 'sidebar id DNE');
          assert.equal(isSidebarShowing(window), false, 'the sidebar is not showing');

          done();
        }
      })

      sidebar.show();
      assert.pass('showing the sidebar');
    }).then(null, assert.fail);
  }).then(null, assert.fail);
}

exports.testShowInPrivateWindow = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testShowInPrivateWindow';
  let window1 = getMostRecentBrowserWindow();
  let url = 'data:text/html;charset=utf-8,'+testName;

  let sidebar1 = Sidebar({
    id: testName,
    title: testName,
    url: url
  });
  let menuitemID = makeID(sidebar1.id);

  assert.equal(sidebar1.url, url, 'url getter works');
  assert.equal(isShowing(sidebar1), false, 'the sidebar is not showing');
  assert.ok(!isChecked(window1.document.getElementById(menuitemID)),
               'the menuitem is not checked');
  assert.equal(isSidebarShowing(window1), false, 'the new window sidebar is not showing');

  windowPromise(window1.OpenBrowserWindow({ private: true }), 'load').then(function(window) {
    let { document } = window;
    assert.equal(isWindowPrivate(window), true, 'new window is private');
    assert.equal(isPrivate(window), true, 'new window is private');

    sidebar1.show().then(
      function good() {
        assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
        assert.ok(!!document.getElementById(menuitemID),
                  'the menuitem exists on the private window');
        assert.equal(isSidebarShowing(window), true, 'the new window sidebar is showing');

        sidebar1.destroy();
        assert.equal(isSidebarShowing(window), false, 'the new window sidebar is showing');
        assert.ok(!window1.document.getElementById(menuitemID),
                  'the menuitem on the new window dne');

        // test old window state
        assert.equal(isSidebarShowing(window1), false, 'the old window sidebar is not showing');
        assert.equal(window1.document.getElementById(menuitemID),
                     null,
                     'the menuitem on the old window dne');

        close(window).then(done).then(null, assert.fail);
      },
      function bad() {
        assert.fail('a successful show should not happen here..');
      });
  }).then(null, assert.fail);
}

// If the module doesn't support the app we're being run in, require() will
// throw.  In that case, remove all tests above from exports, and add one dummy
// test that passes.
try {
  require('sdk/ui/sidebar');
}
catch (err) {
  if (!/^Unsupported Application/.test(err.message))
    throw err;

  module.exports = {
    'test Unsupported Application': assert => assert.pass(err.message)
  }
}
