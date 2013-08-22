/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'engines': {
    'Firefox': '> 24'
  }
};

const { Loader } = require('sdk/test/loader');
const { show, hide } = require('sdk/ui/sidebar/actions');
const { isShowing } = require('sdk/ui/sidebar/utils');
const { getMostRecentBrowserWindow, isWindowPrivate } = require('sdk/window/utils');
const { open, close, focus, promise: windowPromise } = require('sdk/window/helpers');
const { setTimeout } = require('sdk/timers');
const { isPrivate } = require('sdk/private-browsing');
const { data } = require('sdk/self');
const { URL } = require('sdk/url');

const { BLANK_IMG, BUILTIN_SIDEBAR_MENUITEMS, isSidebarShowing,
        getSidebarMenuitems, getExtraSidebarMenuitems, makeID, simulateCommand,
        simulateClick, getWidget, isChecked } = require('./sidebar/utils');

exports.testSideBarIsNotInNewPrivateWindows = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testSideBarIsNotInNewPrivateWindows';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    icon: BLANK_IMG,
    url: 'data:text/html;charset=utf-8,'+testName
  });

  let startWindow = getMostRecentBrowserWindow();
  let ele = startWindow.document.getElementById(makeID(testName));
  assert.ok(ele, 'sidebar element was added');

  open(null, { features: { private: true } }).then(function(window) {
      let ele = window.document.getElementById(makeID(testName));
      assert.ok(isPrivate(window), 'the new window is private');
      assert.equal(ele, null, 'sidebar element was not added');

      sidebar.destroy();
      assert.ok(!window.document.getElementById(makeID(testName)), 'sidebar id DNE');
      assert.ok(!startWindow.document.getElementById(makeID(testName)), 'sidebar id DNE');

      close(window).then(done, assert.fail);
  })
}

/*
exports.testSidebarIsNotOpenInNewPrivateWindow = function(assert, done) {
  let testName = 'testSidebarIsNotOpenInNewPrivateWindow';
  let window = getMostRecentBrowserWindow();

    let sidebar = Sidebar({
      id: testName,
      title: testName,
      icon: BLANK_IMG,
      url: 'data:text/html;charset=utf-8,'+testName
    });

    sidebar.on('show', function() {
      assert.equal(isPrivate(window), false, 'the new window is not private');
      assert.equal(isSidebarShowing(window), true, 'the sidebar is showing');
      assert.equal(isShowing(sidebar), true, 'the sidebar is showing');

      let window2 = window.OpenBrowserWindow({private: true});
      windowPromise(window2, 'load').then(focus).then(function() {
        // TODO: find better alt to setTimeout...
        setTimeout(function() {
          assert.equal(isPrivate(window2), true, 'the new window is private');
          assert.equal(isSidebarShowing(window), true, 'the sidebar is showing in old window still');
          assert.equal(isSidebarShowing(window2), false, 'the sidebar is not showing in the new private window');
          assert.equal(isShowing(sidebar), false, 'the sidebar is not showing');
          sidebar.destroy();
          close(window2).then(done);
        }, 500)
      })
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
    icon: BLANK_IMG,
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
        icon: BLANK_IMG,
        url:  'data:text/html;charset=utf-8,'+ testName,
        onShow: function() {
          assert.pass('onShow works for Sidebar');
          loader.unload();

          let sidebarMI = getSidebarMenuitems();
          for each (let mi in sidebarMI) {
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

    });
  });
}

exports.testShowInPrivateWindow = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testShowInPrivateWindow';
  let window = getMostRecentBrowserWindow();
  let { document } = window;
  let url = 'data:text/html;charset=utf-8,'+testName;

  let sidebar1 = Sidebar({
    id: testName,
    title: testName,
    icon: BLANK_IMG,
    url: url
  });

  assert.equal(sidebar1.url, url, 'url getter works');
  assert.equal(isShowing(sidebar1), false, 'the sidebar is not showing');
  assert.ok(!isChecked(document.getElementById(makeID(sidebar1.id))),
               'the menuitem is not checked');
  assert.equal(isSidebarShowing(window), false, 'the new window sidebar is not showing');

  windowPromise(window.OpenBrowserWindow({ private: true }), 'load').then(function(window) {
    let { document } = window;
    assert.equal(isWindowPrivate(window), true, 'new window is private');
    assert.equal(isPrivate(window), true, 'new window is private');

    sidebar1.show().then(
      function bad() {
        assert.fail('a successful show should not happen here..');
      },
      function good() {
        assert.equal(isShowing(sidebar1), false, 'the sidebar is still not showing');
        assert.equal(document.getElementById(makeID(sidebar1.id)),
                     null,
                     'the menuitem dne on the private window');
        assert.equal(isSidebarShowing(window), false, 'the new window sidebar is not showing');

        sidebar1.destroy();
        close(window).then(done);
      });
  }, assert.fail);
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

require('sdk/test').run(exports);
