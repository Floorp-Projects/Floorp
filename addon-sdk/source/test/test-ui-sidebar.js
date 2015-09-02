/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

module.metadata = {
  'engines': {
    'Firefox': '*'
  }
};

const { Cu } = require('chrome');
const { Loader } = require('sdk/test/loader');
const { show, hide } = require('sdk/ui/sidebar/actions');
const { isShowing } = require('sdk/ui/sidebar/utils');
const { getMostRecentBrowserWindow, isFocused } = require('sdk/window/utils');
const { open, close, focus, promise: windowPromise } = require('sdk/window/helpers');
const { setTimeout, setImmediate } = require('sdk/timers');
const { isPrivate } = require('sdk/private-browsing');
const data = require('./fixtures');
const { URL } = require('sdk/url');
const { once, off, emit } = require('sdk/event/core');
const { defer, all } = require('sdk/core/promise');
const { modelFor } = require('sdk/model/core');
const { cleanUI } = require("sdk/test/utils");
const { before, after } = require('sdk/test/utils');

require('sdk/windows');

const { BUILTIN_SIDEBAR_MENUITEMS, isSidebarShowing,
        getSidebarMenuitems, getExtraSidebarMenuitems, makeID, simulateCommand,
        simulateClick, isChecked } = require('./sidebar/utils');

exports.testSidebarBasicLifeCycle = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testSidebarBasicLifeCycle';
  let window = getMostRecentBrowserWindow();
  assert.ok(!window.document.getElementById(makeID(testName)), 'sidebar id DNE');
  let sidebarXUL = window.document.getElementById('sidebar');
  assert.ok(sidebarXUL, 'sidebar xul element does exist');
  assert.ok(!getExtraSidebarMenuitems().length, 'there are no extra sidebar menuitems');

  assert.equal(isSidebarShowing(window), false, 'sidebar is not showing 1');
  let sidebarDetails = {
    id: testName,
    title: 'test',
    url: 'data:text/html;charset=utf-8,'+testName
  };
  let sidebar = Sidebar(sidebarDetails);

  // test the sidebar attributes
  for (let key of Object.keys(sidebarDetails)) {
    assert.equal(sidebarDetails[key], sidebar[key], 'the attributes match the input');
  }

  assert.pass('The Sidebar constructor worked');

  let extraMenuitems = getExtraSidebarMenuitems();
  assert.equal(extraMenuitems.length, 1, 'there is one extra sidebar menuitems');

  let ele = window.document.getElementById(makeID(testName));
  assert.equal(ele, extraMenuitems[0], 'the only extra menuitem is the one for our sidebar.')
  assert.ok(ele, 'sidebar element was added');
  assert.ok(!isChecked(ele), 'the sidebar is not displayed');
  assert.equal(ele.getAttribute('label'), sidebar.title, 'the sidebar title is the menuitem label')

  assert.equal(isSidebarShowing(window), false, 'sidebar is not showing 2');

  // explicit test of the on hide event
  yield new Promise(resolve => {
    sidebar.on('show', resolve);
    sidebar.show();
    assert.pass('showing sidebar..');
  });

  assert.pass('the show event was fired');
  assert.equal(isSidebarShowing(window), true, 'sidebar is not showing 3');
  assert.equal(isShowing(sidebar), true, 'the sidebar is showing');
  assert.ok(isChecked(ele), 'the sidebar is displayed');

  // explicit test of the on show event
  yield new Promise(resolve => {
    sidebar.on('hide', () => {
      sidebar.once('detach', resolve);

      assert.pass('the hide event was fired');
      assert.ok(!isChecked(ele), 'the sidebar menuitem is not checked');
      assert.equal(isShowing(sidebar), false, 'the sidebar is not showing');
      assert.equal(isSidebarShowing(window), false, 'the sidebar elemnt is hidden');
    });
    sidebar.hide();
    assert.pass('hiding sidebar..');
  });

  // calling destroy twice should not matter
  sidebar.destroy();
  sidebar.destroy();

  for (let mi of getSidebarMenuitems()) {
    let id = mi.getAttribute('id');

    if (BUILTIN_SIDEBAR_MENUITEMS.indexOf(id) < 0) {
      assert.fail('the menuitem "' + id + '" is not a built-in sidebar');
    }
    assert.ok(!isChecked(mi), 'no sidebar menuitem is checked');
  }

  assert.ok(!window.document.getElementById(makeID(testName)), 'sidebar id DNE');
  assert.pass('calling destroy worked without error');
}

exports.testSideBarIsInNewWindows = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testSideBarIsInNewWindows';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,'+testName
  });

  let startWindow = getMostRecentBrowserWindow();
  let ele = startWindow.document.getElementById(makeID(testName));
  assert.ok(ele, 'sidebar element was added');

  let window = yield open();

  ele = window.document.getElementById(makeID(testName));
  assert.ok(ele, 'sidebar element was added');

  // calling destroy twice should not matter
  sidebar.destroy();
  sidebar.destroy();

  assert.ok(!window.document.getElementById(makeID(testName)), 'sidebar id DNE');
  assert.ok(!startWindow.document.getElementById(makeID(testName)), 'sidebar id DNE');
}

exports.testSideBarIsShowingInNewWindows = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testSideBarIsShowingInNewWindows';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: URL('data:text/html;charset=utf-8,'+testName)
  });

  let startWindow = getMostRecentBrowserWindow();
  let ele = startWindow.document.getElementById(makeID(testName));
  assert.ok(ele, 'sidebar element was added');

  let oldEle = ele;

  yield new Promise(resolve => {
    sidebar.once('attach', function() {
      assert.pass('attach event fired');

      sidebar.once('show', function() {
        assert.pass('show event fired');
        resolve();
      })
    });
    sidebar.show();
  });

  yield new Promise(resolve => {
    sidebar.once('show', resolve);
    startWindow.OpenBrowserWindow();
  });

  let window = getMostRecentBrowserWindow();
  assert.notEqual(startWindow, window, 'window is new');

  let sb = window.document.getElementById('sidebar');
  yield new Promise(resolve => {
    if (sb && sb.docShell && sb.contentDocument && sb.contentDocument.getElementById('web-panels-browser')) {
      end();
    }
    else {
      sb.addEventListener('DOMWindowCreated', end, false);
    }
    function end () {
      sb.removeEventListener('DOMWindowCreated', end, false);
      resolve();
    }
  })

  ele = window.document.getElementById(makeID(testName));
  assert.ok(ele, 'sidebar element was added 2');
  assert.ok(isChecked(ele), 'the sidebar is checked');
  assert.notEqual(ele, oldEle, 'there are two different sidebars');

  assert.equal(isShowing(sidebar), true, 'the sidebar is showing in new window');

  sidebar.destroy();

  assert.equal(isShowing(sidebar), false, 'the sidebar is not showing');
  assert.ok(!isSidebarShowing(window), 'sidebar in most recent window is not showing');
  assert.ok(!isSidebarShowing(startWindow), 'sidebar in most start window is not showing');
  assert.ok(!window.document.getElementById(makeID(testName)), 'sidebar id DNE');
  assert.ok(!startWindow.document.getElementById(makeID(testName)), 'sidebar id DNE');
}

// TODO: determine if this is acceptable..
/*
exports.testAddonGlobalSimple = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testAddonGlobalSimple';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: data.url('test-sidebar-addon-global.html')
  });

  sidebar.on('show', function({worker}) {
    assert.pass('sidebar was attached');
    assert.ok(!!worker, 'attach event has worker');

    worker.port.on('X', function(msg) {
      assert.equal(msg, '23', 'the final message is correct');

      sidebar.destroy();

      done();
    });
    worker.port.emit('X', '2');
  });
  show(sidebar);
}
*/

exports.testAddonGlobalComplex = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testAddonGlobalComplex';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: data.url('test-sidebar-addon-global.html')
  });

  let worker = yield new Promise(resolve => {
    sidebar.on('attach', resolve);
    show(sidebar);
  });

  assert.pass('sidebar was attached');
  assert.ok(!!worker, 'attach event has worker');


  let msg = yield new Promise(resolve => {
    worker.port.once('Y', resolve);
  });

  assert.equal(msg, '1', 'got event from worker');

  msg = yield new Promise(resolve => {
    worker.port.on('X', resolve);
    worker.port.emit('X', msg + '2');
  });

  assert.equal(msg, '123', 'the final message is correct');

  sidebar.destroy();
}

exports.testAddonReady = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testAddonReady';
  let ready = defer();
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: data.url('test-sidebar-addon-global.html'),
    onReady: ready.resolve
  });
  show(sidebar);

  let worker = yield ready.promise;
  assert.pass('sidebar was attached');
  assert.ok(!!worker, 'attach event has worker');

  let msg = yield new Promise(resolve => {
    worker.port.on('X', resolve);
    worker.port.emit('X', '12');
  });
  assert.equal(msg, '123', 'the final message is correct');

  sidebar.destroy();
}

exports.testShowingOneSidebarAfterAnother = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testShowingOneSidebarAfterAnother';

  let sidebar1 = Sidebar({
    id: testName + '1',
    title: testName + '1',
    url:  'data:text/html;charset=utf-8,'+ testName + 1
  });
  let sidebar2 = Sidebar({
    id: testName + '2',
    title: testName + '2',
    url:  'data:text/html;charset=utf-8,'+ testName + 2
  });

  let window = getMostRecentBrowserWindow();
  let IDs = [ sidebar1.id, sidebar2.id ];

  let extraMenuitems = getExtraSidebarMenuitems(window);
  assert.equal(extraMenuitems.length, 2, 'there are two extra sidebar menuitems');

  function testShowing(sb1, sb2, sbEle) {
    assert.equal(isShowing(sidebar1), sb1);
    assert.equal(isShowing(sidebar2), sb2);
    assert.equal(isSidebarShowing(window), sbEle);
  }
  testShowing(false, false, false);

  yield show(sidebar1);
  assert.pass('showed sidebar1');

  testShowing(true, false, true);

  for (let mi of getExtraSidebarMenuitems(window)) {
    let menuitemID = mi.getAttribute('id').replace(/^jetpack-sidebar-/, '');
    assert.ok(IDs.indexOf(menuitemID) >= 0, 'the extra menuitem is for one of our test sidebars');
    assert.equal(isChecked(mi), menuitemID == sidebar1.id, 'the test sidebar menuitem has the correct checked value');
  }

  yield show(sidebar2);
  assert.pass('showed sidebar2');

  testShowing(false, true, true);

  for (let mi of getExtraSidebarMenuitems(window)) {
    let menuitemID = mi.getAttribute('id').replace(/^jetpack-sidebar-/, '');
    assert.ok(IDs.indexOf(menuitemID) >= 0, 'the extra menuitem is for one of our test sidebars');
    assert.equal(isChecked(mi), menuitemID == sidebar2.id, 'the test sidebar menuitem has the correct checked value');
  }

  sidebar1.destroy();
  sidebar2.destroy();

  testShowing(false, false, false);
}

exports.testSidebarUnload = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testSidebarUnload';
  let loader = Loader(module);

  let window = getMostRecentBrowserWindow();

  assert.equal(isPrivate(window), false, 'the current window is not private');

  let sidebar = loader.require('sdk/ui/sidebar').Sidebar({
    id: testName,
    title: testName,
    url:  'data:text/html;charset=utf-8,'+ testName
  });

  yield sidebar.show();
  assert.pass('showing the sidebar');

  loader.unload();

  for (let mi of getSidebarMenuitems()) {
    assert.ok(BUILTIN_SIDEBAR_MENUITEMS.indexOf(mi.getAttribute('id')) >= 0, 'the menuitem is for a built-in sidebar')
    assert.ok(!isChecked(mi), 'no sidebar menuitem is checked');
  }

  assert.ok(!window.document.getElementById(makeID(testName)), 'sidebar id DNE');
  assert.equal(isSidebarShowing(window), false, 'the sidebar is not showing');
}

exports.testRelativeURL = function*(assert) {
  const { merge } = require('sdk/util/object');
  const self = require('sdk/self');

  let loader = Loader(module, null, null, {
    modules: {
      'sdk/self': merge({}, self, {
        data: merge({}, self.data, require('./fixtures'))
      })
    }
  });

  const { Sidebar } = loader.require('sdk/ui/sidebar');

  let testName = 'testRelativeURL';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: './test-sidebar-addon-global.html'
  });

  yield new Promise(resolve => {
    sidebar.on('attach', function(worker) {
      assert.pass('sidebar was attached');
      assert.ok(!!worker, 'attach event has worker');

      worker.port.once('Y', function(msg) {
        assert.equal(msg, '1', 'got event from worker');

        worker.port.on('X', function(msg) {
          assert.equal(msg, '123', 'the final message is correct');
          resolve();
        });
        worker.port.emit('X', msg + '2');
      })
    });
    sidebar.show();
  });

  sidebar.destroy();
}

exports.testRemoteContent = function(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testRemoteContent';
  try {
    let sidebar = Sidebar({
      id: testName,
      title: testName,
      url: 'http://dne.xyz.mozilla.org'
    });
    assert.fail('a bad sidebar was created..');
    sidebar.destroy();
  }
  catch(e) {
    assert.ok(/The option "url" must be a valid local URI\./.test(e), 'remote content is not acceptable');
  }
}

exports.testInvalidURL = function(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testInvalidURL';
  try {
    let sidebar = Sidebar({
      id: testName,
      title: testName,
      url: 'http:mozilla.org'
    });
    assert.fail('a bad sidebar was created..');
    sidebar.destroy();
  }
  catch(e) {
    assert.ok(/The option "url" must be a valid local URI\./.test(e), 'invalid URIs are not acceptable');
  }
}

exports.testInvalidURLType = function(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testInvalidURLType';
  try {
    let sidebar = Sidebar({
      id: testName,
      title: testName
    });
    assert.fail('a bad sidebar was created..');
    sidebar.destroy();
  }
  catch(e) {
    assert.ok(/The option "url" must be a valid local URI\./.test(e), 'invalid URIs are not acceptable');
  }
}

exports.testInvalidTitle = function(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testInvalidTitle';
  try {
    let sidebar = Sidebar({
      id: testName,
      title: '',
      url: 'data:text/html;charset=utf-8,'+testName
    });
    assert.fail('a bad sidebar was created..');
    sidebar.destroy();
  }
  catch(e) {
    assert.equal('The option "title" must be one of the following types: string', e.message, 'invalid titles are not acceptable');
  }
}

exports.testInvalidID = function(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testInvalidID';
  try {
    let sidebar = Sidebar({
      id: '!',
      title: testName,
      url: 'data:text/html;charset=utf-8,'+testName
    });
    assert.fail('a bad sidebar was created..');
    sidebar.destroy();
  }
  catch(e) {
    assert.ok(/The option "id" must be a valid alphanumeric id/.test(e), 'invalid ids are not acceptable');
  }
}

exports.testInvalidBlankID = function(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testInvalidBlankID';
  try {
    let sidebar = Sidebar({
      id: '',
      title: testName,
      url: 'data:text/html;charset=utf-8,'+testName
    });
    assert.fail('a bad sidebar was created..');
    sidebar.destroy();
  }
  catch(e) {
    assert.ok(/The option "id" must be a valid alphanumeric id/.test(e), 'invalid ids are not acceptable');
  }
}

exports.testInvalidNullID = function(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testInvalidNullID';
  try {
    let sidebar = Sidebar({
      id: null,
      title: testName,
      url: 'data:text/html;charset=utf-8,'+testName
    });
    assert.fail('a bad sidebar was created..');
    sidebar.destroy();
  }
  catch(e) {
    assert.ok(/The option "id" must be a valid alphanumeric id/.test(e), 'invalid ids are not acceptable');
  }
}

exports.testUndefinedID = function(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testInvalidUndefinedID';

  try {
    let sidebar = Sidebar({
      title: testName,
      url: 'data:text/html;charset=utf-8,' + testName
    });

    assert.ok(sidebar.id, 'an undefined id was accepted, id was creawted: ' + sidebar.id);
    assert.ok(getMostRecentBrowserWindow().document.getElementById(makeID(sidebar.id)), 'the sidebar element was found');

    sidebar.destroy();
  }
  catch(e) {
    assert.fail('undefined ids are acceptable');
    assert.fail(e.message);
  }
}

// TEST: edge case where web panel is destroyed while loading
exports.testDestroyEdgeCaseBug = function*(assert) {
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

  let window2 = yield open().then(focus);

  assert.equal(isPrivate(window2), false, 'the new window is not private');
  assert.equal(isSidebarShowing(window2), false, 'the sidebar is not showing');
  assert.equal(isShowing(sidebar), false, 'the sidebar is not showing');

  sidebar.destroy();
  assert.pass('destroying the sidebar');

  yield close(window2);

  let loader = Loader(module);

  assert.equal(isPrivate(window), false, 'the current window is not private');

  sidebar = loader.require('sdk/ui/sidebar').Sidebar({
    id: testName,
    title: testName,
    url:  'data:text/html;charset=utf-8,'+ testName,
    onShow: function() {
    }
  })

  assert.pass('showing the sidebar');
  yield sidebar.show();
  loader.unload();

  for (let mi of getSidebarMenuitems()) {
    let id = mi.getAttribute('id');

    if (BUILTIN_SIDEBAR_MENUITEMS.indexOf(id) < 0) {
      assert.fail('the menuitem "' + id + '" is not a built-in sidebar');
    }
    assert.ok(!isChecked(mi), 'no sidebar menuitem is checked');
  }
  assert.ok(!window.document.getElementById(makeID(testName)), 'sidebar id DNE');
  assert.equal(isSidebarShowing(window), false, 'the sidebar is not showing');
}

exports.testClickingACheckedMenuitem = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  const testName = 'testClickingACheckedMenuitem';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,'+testName,
  });
  assert.pass('sidebar was created');

  let window = yield open().then(focus);
  yield sidebar.show();
  assert.pass('the show callback works');

  let waitForHide = defer();
  sidebar.once('hide', waitForHide.resolve);
  let menuitem = window.document.getElementById(makeID(sidebar.id));
  simulateCommand(menuitem);

  yield waitForHide.promise;

  assert.pass('clicking the menuitem after the sidebar has shown hides it.');
  sidebar.destroy();
};

exports.testTitleSetter = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testTitleSetter';
  let { document } = getMostRecentBrowserWindow();

  let sidebar1 = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,'+testName,
  });

  assert.equal(sidebar1.title, testName, 'title getter works');

  yield sidebar1.show();
  assert.equal(document.getElementById(makeID(sidebar1.id)).getAttribute('label'),
               testName,
               'the menuitem label is correct');

  assert.equal(document.getElementById('sidebar-title').value, testName, 'the menuitem label is correct');

  sidebar1.title = 'foo';

  assert.equal(sidebar1.title, 'foo', 'title getter works');

  assert.equal(document.getElementById(makeID(sidebar1.id)).getAttribute('label'),
               'foo',
               'the menuitem label was updated');

  assert.equal(document.getElementById('sidebar-title').value, 'foo', 'the sidebar title was updated');

  sidebar1.destroy();
}

exports.testURLSetter = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testURLSetter';
  let window = getMostRecentBrowserWindow();
  let { document } = window;
  let url = 'data:text/html;charset=utf-8,'+testName;

  let sidebar1 = Sidebar({
    id: testName,
    title: testName,
    url: url
  });

  assert.equal(sidebar1.url, url, 'url getter works');
  assert.equal(isShowing(sidebar1), false, 'the sidebar is not showing');
  assert.ok(!isChecked(document.getElementById(makeID(sidebar1.id))),
               'the menuitem is not checked');
  assert.equal(isSidebarShowing(window), false, 'the new window sidebar is not showing');

  window = yield windowPromise(window.OpenBrowserWindow(), 'load');
  document = window.document;
  assert.pass('new window was opened');

  yield sidebar1.show();

  assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
  assert.ok(isChecked(document.getElementById(makeID(sidebar1.id))),
               'the menuitem is checked');
  assert.ok(isSidebarShowing(window), 'the new window sidebar is showing');

  yield new Promise(resolve => {
    sidebar1.once('show', resolve);
    sidebar1.url = (url + '1');
    assert.equal(sidebar1.url, (url + '1'), 'url getter works');
    assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
    assert.ok(isSidebarShowing(window), 'the new window sidebar is showing');
  });

  assert.pass('setting the sidebar.url causes a show event');

  assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
  assert.equal(isSidebarShowing(window), true, 'the new window sidebar is still showing');

  assert.ok(isChecked(document.getElementById(makeID(sidebar1.id))),
               'the menuitem is still checked');

  sidebar1.destroy();
  assert.equal(isSidebarShowing(window), false, 'the new window sidebar is not showing');
}

exports.testDuplicateID = function(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testDuplicateID';
  let window = getMostRecentBrowserWindow();
  let { document } = window;
  let url = 'data:text/html;charset=utf-8,'+testName;

  let sidebar1 = Sidebar({
    id: testName,
    title: testName,
    url: url
  });

  assert.throws(function() {
    Sidebar({
      id: testName,
      title: testName + 1,
      url: url + 2
    }).destroy();
  }, /The ID .+ seems already used\./i, 'duplicate IDs will throw errors');

  sidebar1.destroy();
}

exports.testURLSetterToSameValueReloadsSidebar = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testURLSetterToSameValueReloadsSidebar';
  let window = getMostRecentBrowserWindow();
  let { document } = window;
  let url = 'data:text/html;charset=utf-8,'+testName;

  let sidebar1 = Sidebar({
    id: testName,
    title: testName,
    url: url
  });

  assert.equal(sidebar1.url, url, 'url getter works');
  assert.equal(isShowing(sidebar1), false, 'the sidebar is not showing');
  assert.ok(!isChecked(document.getElementById(makeID(sidebar1.id))),
               'the menuitem is not checked');
  assert.equal(isSidebarShowing(window), false, 'the new window sidebar is not showing');

  window = yield windowPromise(window.OpenBrowserWindow(), 'load');
  document = window.document;
  assert.pass('new window was opened');

  yield focus(window);
  assert.pass('new window was focused');

  yield sidebar1.show();

  assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
  assert.ok(isChecked(document.getElementById(makeID(sidebar1.id))),
               'the menuitem is checked');
  assert.ok(isSidebarShowing(window), 'the new window sidebar is showing');

  let shown = defer();
  sidebar1.once('show', shown.resolve);
  sidebar1.url = url;

  assert.equal(sidebar1.url, url, 'url getter works');
  assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
  assert.ok(isSidebarShowing(window), 'the new window sidebar is showing');

  yield shown.promise;

  assert.pass('setting the sidebar.url causes a show event');

  assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
  assert.ok(isSidebarShowing(window), 'the new window sidebar is still showing');

  assert.ok(isChecked(document.getElementById(makeID(sidebar1.id))),
               'the menuitem is still checked');

  sidebar1.destroy();
}

exports.testShowingInOneWindowDoesNotAffectOtherWindows = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testShowingInOneWindowDoesNotAffectOtherWindows';
  let window1 = getMostRecentBrowserWindow();
  let url = 'data:text/html;charset=utf-8,'+testName;

  let sidebar1 = Sidebar({
    id: testName,
    title: testName,
    url: url
  });

  assert.equal(sidebar1.url, url, 'url getter works');
  assert.equal(isShowing(sidebar1), false, 'the sidebar is not showing');
  let checkCount = 1;
  function checkSidebarShowing(window, expected) {
    assert.pass('check count ' + checkCount++);

    let mi = window.document.getElementById(makeID(sidebar1.id));
    if (mi) {
      assert.equal(isChecked(mi), expected,
                   'the menuitem is not checked');
    }
    assert.equal(isSidebarShowing(window), expected || false, 'the new window sidebar is not showing');
  }
  checkSidebarShowing(window1, false);

  let window = yield windowPromise(window1.OpenBrowserWindow(), 'load');
  let { document } = window;
  assert.pass('new window was opened!');

  // waiting for show
  yield sidebar1.show();

  // check state of the new window
  assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
  checkSidebarShowing(window, true);

  // check state of old window
  checkSidebarShowing(window1, false);

  yield sidebar1.show();

  // check state of the new window
  assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
  checkSidebarShowing(window, true);

  // check state of old window
  checkSidebarShowing(window1, false);

  // calling destroy() twice should not matter
  sidebar1.destroy();
  sidebar1.destroy();

  // check state of the new window
  assert.equal(isShowing(sidebar1), false, 'the sidebar is not showing');
  checkSidebarShowing(window, undefined);

  // check state of old window
  checkSidebarShowing(window1, undefined);
}

exports.testHidingAHiddenSidebarRejects = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testHidingAHiddenSidebarRejects';
  let url = 'data:text/html;charset=utf-8,'+testName;
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: url
  });

  yield sidebar.hide().then(assert.fail, assert.pass);

  sidebar.destroy();
}

exports.testGCdSidebarsOnUnload = function*(assert) {
  const loader = Loader(module);
  const { Sidebar } = loader.require('sdk/ui/sidebar');
  const window = getMostRecentBrowserWindow();

  let testName = 'testGCdSidebarsOnUnload';
  let url = 'data:text/html;charset=utf-8,'+testName;

  assert.equal(isSidebarShowing(window), false, 'the sidebar is not showing');

  // IMPORTANT: make no reference to the sidebar instance, so it is GC'd
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: url
  });

  yield sidebar.show();
  sidebar = null;

  assert.equal(isSidebarShowing(window), true, 'the sidebar is showing');

  let menuitemID = makeID(testName);

  assert.ok(window.document.getElementById(menuitemID), 'the menuitem was found');

  yield new Promise(resolve => Cu.schedulePreciseGC(resolve));

  loader.unload();

  assert.equal(isSidebarShowing(window), false, 'the sidebar is not showing after unload');
  assert.ok(!window.document.getElementById(menuitemID), 'the menuitem was removed');
}

exports.testGCdShowingSidebarsOnUnload = function*(assert) {
  const loader = Loader(module);
  const { Sidebar } = loader.require('sdk/ui/sidebar');
  const window = getMostRecentBrowserWindow();

  let testName = 'testGCdShowingSidebarsOnUnload';
  let url = 'data:text/html;charset=utf-8,'+testName;

  assert.equal(isSidebarShowing(window), false, 'the sidebar is not showing');

  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: url
  });

  yield sidebar.show();

  sidebar = null;

  assert.equal(isSidebarShowing(window), true, 'the sidebar is showing');

  let menuitemID = makeID(testName);

  assert.ok(!!window.document.getElementById(menuitemID), 'the menuitem was found');

  yield new Promise(resolve => Cu.schedulePreciseGC(resolve));

  assert.equal(isSidebarShowing(window), true, 'the sidebar is still showing after gc');
  assert.ok(!!window.document.getElementById(menuitemID), 'the menuitem was found after gc');

  loader.unload();

  assert.equal(isSidebarShowing(window), false, 'the sidebar is not showing after unload');
  assert.ok(!window.document.getElementById(menuitemID), 'the menuitem was removed');
}

exports.testDetachEventOnWindowClose = function*(assert) {
  const loader = Loader(module);
  const { Sidebar } = loader.require('sdk/ui/sidebar');
  let window = getMostRecentBrowserWindow();

  let testName = 'testDetachEventOnWindowClose';
  let url = 'data:text/html;charset=utf-8,' + testName;

  window = yield windowPromise(window.OpenBrowserWindow(), 'load').then(focus);

  yield new Promise(resolve => {
    let sidebar = Sidebar({
      id: testName,
      title: testName,
      url: url,
      onAttach: function() {
        assert.pass('the attach event is fired');
        window.close();
      },
      onDetach: resolve
    });

    sidebar.show();
  });

  assert.pass('the detach event is fired when the window showing it closes');
  loader.unload();
}

exports.testHideEventOnWindowClose = function*(assert) {
  const loader = Loader(module);
  const { Sidebar } = loader.require('sdk/ui/sidebar');
  let window = getMostRecentBrowserWindow();

  let testName = 'testDetachEventOnWindowClose';
  let url = 'data:text/html;charset=utf-8,' + testName;


  window = yield windowPromise(window.OpenBrowserWindow(), 'load').then(focus);
  yield new Promise(resolve => {
    let sidebar = Sidebar({
      id: testName,
      title: testName,
      url: url,
      onAttach: function() {
        assert.pass('the attach event is fired');
        window.close();
      },
      onHide: resolve
    });

    sidebar.show();
  });

  assert.pass('the hide event is fired when the window showing it closes');
  loader.unload();
}

exports.testGCdHiddenSidebarsOnUnload = function*(assert) {
  const loader = Loader(module);
  const { Sidebar } = loader.require('sdk/ui/sidebar');
  const window = getMostRecentBrowserWindow();

  let testName = 'testGCdHiddenSidebarsOnUnload';
  let url = 'data:text/html;charset=utf-8,'+testName;

  assert.equal(isSidebarShowing(window), false, 'the sidebar is not showing');

  // IMPORTANT: make no reference to the sidebar instance, so it is GC'd
  Sidebar({
    id: testName,
    title: testName,
    url: url
  });

  let menuitemID = makeID(testName);

  assert.ok(!!window.document.getElementById(menuitemID), 'the menuitem was found');

  yield new Promise(resolve => Cu.schedulePreciseGC(resolve));

  assert.ok(!!window.document.getElementById(menuitemID), 'the menuitem was found after gc');

  loader.unload();

  assert.ok(!window.document.getElementById(menuitemID), 'the menuitem was removed');
}

exports.testSidebarGettersAndSettersAfterDestroy = function(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testSidebarGettersAndSettersAfterDestroy';
  let url = 'data:text/html;charset=utf-8,'+testName;

  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: url
  });

  sidebar.destroy();

  assert.equal(sidebar.id, undefined, 'sidebar after destroy has no id');

  assert.throws(() => sidebar.id = 'foo-tang',
    /^setting a property that has only a getter/,
    'id cannot be set at runtime');

  assert.equal(sidebar.id, undefined, 'sidebar after destroy has no id');

  assert.equal(sidebar.title, undefined, 'sidebar after destroy has no title');
  sidebar.title = 'boo-tang';
  assert.equal(sidebar.title, undefined, 'sidebar after destroy has no title');

  assert.equal(sidebar.url, undefined, 'sidebar after destroy has no url');
  sidebar.url = url + 'barz';
  assert.equal(sidebar.url, undefined, 'sidebar after destroy has no url');
}


exports.testSidebarLeakCheckDestroyAfterAttach = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testSidebarLeakCheckDestroyAfterAttach';
  let window = getMostRecentBrowserWindow();
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,'+testName
  });

  yield new Promise(resolve => {
    sidebar.on('attach', resolve);
    assert.pass('showing the sidebar');
    sidebar.show();
  })

  assert.pass('the sidebar was shown');

  sidebar.on('show', () => {
    assert.fail('the sidebar show listener should have been removed');
  });
  assert.pass('added a sidebar show listener');

  sidebar.on('hide', () => {
    assert.fail('the sidebar hide listener should have been removed');
  });
  assert.pass('added a sidebar hide listener');

  yield new Promise(resolve => {
    let panelBrowser = window.document.getElementById('sidebar').contentDocument.getElementById('web-panels-browser');
    panelBrowser.contentWindow.addEventListener('unload', function onUnload() {
      panelBrowser.contentWindow.removeEventListener('unload', onUnload, false);
      resolve();
    }, false);
    sidebar.destroy();
  });

  assert.pass('the sidebar web panel was unloaded properly');
}

exports.testSidebarLeakCheckUnloadAfterAttach = function*(assert) {
  const loader = Loader(module);
  const { Sidebar } = loader.require('sdk/ui/sidebar');
  let testName = 'testSidebarLeakCheckUnloadAfterAttach';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,'+testName
  });

  let window = yield open().then(focus);

  yield new Promise(resolve => {
    sidebar.on('attach', resolve);
    assert.pass('showing the sidebar');
    sidebar.show();
  });

  assert.pass('the sidebar was shown');

  sidebar.on('show', function() {
    assert.fail('the sidebar show listener should have been removed');
  });
  assert.pass('added a sidebar show listener');

  sidebar.on('hide', function() {
    assert.fail('the sidebar hide listener should have been removed');
  });
  assert.pass('added a sidebar hide listener');

  let panelBrowser = window.document.getElementById('sidebar').contentDocument.getElementById('web-panels-browser');
  yield new Promise(resolve => {
    panelBrowser.contentWindow.addEventListener('unload', function onUnload() {
      panelBrowser.contentWindow.removeEventListener('unload', onUnload, false);
      resolve();
    }, false);
    loader.unload();
  });

  assert.pass('the sidebar web panel was unloaded properly');
}

exports.testTwoSidebarsWithSameTitleAndURL = function(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testTwoSidebarsWithSameTitleAndURL';

  let title = testName;
  let url = 'data:text/html;charset=utf-8,' + testName;

  let sidebar1 = Sidebar({
    id: testName + 1,
    title: title,
    url: url
  });

  assert.throws(function() {
    Sidebar({
      id: testName + 2,
      title: title,
      url: url
    }).destroy();
  }, /title.+url.+invalid/i, 'Creating two sidebars with the same title + url is not allowed');

  let sidebar2 = Sidebar({
    id: testName + 2,
    title: title,
    url: 'data:text/html;charset=utf-8,X'
  });

  assert.throws(function() {
    sidebar2.url = url;
  }, /title.+url.+invalid/i, 'Creating two sidebars with the same title + url is not allowed');

  sidebar2.title = 'foo';
  sidebar2.url = url;

  assert.throws(function() {
    sidebar2.title = title;
  }, /title.+url.+invalid/i, 'Creating two sidebars with the same title + url is not allowed');

  sidebar1.destroy();
  sidebar2.destroy();
}

exports.testChangingURLBackToOriginalValue = function(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testChangingURLBackToOriginalValue';

  let title = testName;
  let url = 'data:text/html;charset=utf-8,' + testName;
  let count = 0;

  let sidebar = Sidebar({
    id: testName,
    title: title,
    url: url
  });

  sidebar.url = url + 2;
  assert.equal(sidebar.url, url + 2, 'the sidebar.url is correct');
  sidebar.url = url;
  assert.equal(sidebar.url, url, 'the sidebar.url is correct');

  sidebar.title = 'foo';
  assert.equal(sidebar.title, 'foo', 'the sidebar.title is correct');
  sidebar.title = title;
  assert.equal(sidebar.title, title, 'the sidebar.title is correct');

  sidebar.destroy();

  assert.pass('Changing values back to originals works');
}

exports.testShowToOpenXToClose = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testShowToOpenXToClose';

  let title = testName;
  let url = 'data:text/html;charset=utf-8,' + testName;
  let window = getMostRecentBrowserWindow();
  let shown = defer();
  let hidden = defer();

  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: url,
    onShow: shown.resolve,
    onHide: hidden.resolve
  });

  let menuitem = window.document.getElementById(makeID(sidebar.id));

  assert.ok(!isChecked(menuitem), 'menuitem is not checked');

  sidebar.show();

  yield shown.promise;

  assert.ok(isChecked(menuitem), 'menuitem is checked');

  let closeButton = window.document.querySelector('#sidebar-header > toolbarbutton.close-icon');
  simulateCommand(closeButton);

  yield hidden.promise;

  assert.ok(!isChecked(menuitem), 'menuitem is not checked');

  sidebar.destroy();
}

exports.testShowToOpenMenuitemToClose = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testShowToOpenMenuitemToClose';

  let title = testName;
  let url = 'data:text/html;charset=utf-8,' + testName;
  let window = getMostRecentBrowserWindow();

  let hidden = defer();
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: url,
    onHide: hidden.resolve
  });

  let menuitem = window.document.getElementById(makeID(sidebar.id));

  assert.ok(!isChecked(menuitem), 'menuitem is not checked');

  yield sidebar.show();

  assert.ok(isChecked(menuitem), 'menuitem is checked');

  simulateCommand(menuitem);

  yield hidden.promise;

  assert.ok(!isChecked(menuitem), 'menuitem is not checked');

  sidebar.destroy();
}

exports.testDestroyWhileNonBrowserWindowIsOpen = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testDestroyWhileNonBrowserWindowIsOpen';
  let url = 'data:text/html;charset=utf-8,' + testName;

  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: url
  });

  let window = yield open('chrome://browser/content/aboutDialog.xul');

  yield sidebar.show();
  assert.equal(isSidebarShowing(getMostRecentBrowserWindow()), true, 'the sidebar is showing');

  sidebar.destroy();
  assert.pass('sidebar was destroyed while a non browser window was open');

  yield cleanUI();
  assert.equal(isSidebarShowing(getMostRecentBrowserWindow()), false, 'the sidebar is not showing');
}

exports.testEventListeners = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testWhatThisIsInSidebarEventListeners';
  let eventListenerOrder = [];

  let constructorOnShow = defer();
  let constructorOnHide = defer();
  let constructorOnAttach = defer();
  let constructorOnReady = defer();

  let onShow = defer();
  let onHide = defer();
  let onAttach = defer();
  let onReady = defer();

  let onceShow = defer();
  let onceHide = defer();
  let onceAttach = defer();
  let onceReady = defer();

  function testThis() {
    assert(this, sidebar, '`this` is correct');
  }

  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,' + testName,
    onShow: function() {
      assert.equal(this, sidebar, '`this` is correct in onShow');
      eventListenerOrder.push('onShow');
      constructorOnShow.resolve();
    },
    onAttach: function() {
      assert.equal(this, sidebar, '`this` is correct in onAttach');
      eventListenerOrder.push('onAttach');
      constructorOnAttach.resolve();
    },
    onReady: function() {
      assert.equal(this, sidebar, '`this` is correct in onReady');
      eventListenerOrder.push('onReady');
      constructorOnReady.resolve();
    },
    onHide: function() {
      assert.equal(this, sidebar, '`this` is correct in onHide');
      eventListenerOrder.push('onHide');
      constructorOnHide.resolve();
    }
  });

  sidebar.once('show', function() {
    assert.equal(this, sidebar, '`this` is correct in once show');
    eventListenerOrder.push('once show');
    onceShow.resolve();
  });
  sidebar.once('attach', function() {
    assert.equal(this, sidebar, '`this` is correct in once attach');
    eventListenerOrder.push('once attach');
    onceAttach.resolve();
  });
  sidebar.once('ready', function() {
    assert.equal(this, sidebar, '`this` is correct in once ready');
    eventListenerOrder.push('once ready');
    onceReady.resolve();
  });
  sidebar.once('hide', function() {
    assert.equal(this, sidebar, '`this` is correct in once hide');
    eventListenerOrder.push('once hide');
    onceHide.resolve();
  });

  sidebar.on('show', function() {
    assert.equal(this, sidebar, '`this` is correct in on show');
    eventListenerOrder.push('on show');
    onShow.resolve();

    sidebar.hide();
  });
  sidebar.on('attach', function() {
    assert.equal(this, sidebar, '`this` is correct in on attach');
    eventListenerOrder.push('on attach');
    onAttach.resolve();
  });
  sidebar.on('ready', function() {
    assert.equal(this, sidebar, '`this` is correct in on ready');
    eventListenerOrder.push('on ready');
    onReady.resolve();
  });
  sidebar.on('hide', function() {
    assert.equal(this, sidebar, '`this` is correct in on hide');
    eventListenerOrder.push('on hide');
    onHide.resolve();
  });

  sidebar.show();

  yield all([constructorOnShow.promise,
      constructorOnAttach.promise,
      constructorOnReady.promise,
      constructorOnHide.promise,
      onceShow.promise,
      onceAttach.promise,
      onceReady.promise,
      onceHide.promise,
      onShow.promise,
      onAttach.promise,
      onReady.promise,
      onHide.promise]);

  assert.equal(eventListenerOrder.join(), [
    'onAttach',
    'once attach',
    'on attach',
    'onReady',
    'once ready',
    'on ready',
    'onShow',
    'once show',
    'on show',
    'onHide',
    'once hide',
    'on hide'
  ].join(), 'the event order was correct');

  sidebar.destroy();
}

// For more information see Bug 920780
exports.testAttachDoesNotEmitWhenShown = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testAttachDoesNotEmitWhenShown';
  let count = 0;

  let attached = defer();
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,'+testName,
    onAttach: function() {
      if (count > 2) {
        assert.fail('sidebar was attached again..');
      }
      else {
        assert.pass('sidebar was attached ' + count + ' time(s)');
      }

      count++;
      attached.resolve();
    }
  });

  sidebar.show();

  yield attached.promise;

  let shownFired = 0;
  let onShow = () => shownFired++;
  sidebar.on('show', onShow);

  yield sidebar.show();
  assert.equal(shownFired, 0, 'shown should not be fired again when already showing from after attach');

  yield sidebar.hide();
  assert.pass("the sidebar was hidden");

  yield sidebar.show();
  assert.equal(shownFired, 1, 'shown was emitted when `show` called after being hidden');

  yield sidebar.show();
  assert.equal(shownFired, 1, 'shown was not emitted again if already being shown');

  sidebar.off('show', onShow);
  sidebar.destroy();
}

exports.testShowHideRawWindowArg = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');

  let testName = 'testShowHideRawWindowArg';

  assert.pass("Creating sidebar");

  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,' + testName
  });

  assert.pass("Created sidebar");

  let mainWindow = getMostRecentBrowserWindow();
  let newWindow = yield windowPromise(mainWindow.OpenBrowserWindow(), 'load');
  assert.pass("Created the new window");

  yield focus(newWindow);
  assert.pass("Focused the new window");

  let newWindow2 = yield windowPromise(mainWindow.OpenBrowserWindow(), 'load');
  assert.pass("Created the second new window");

  yield focus(newWindow2);
  assert.pass("Focused the second new window");

  yield sidebar.show(newWindow);

  assert.pass('the sidebar was shown');
  assert.equal(isSidebarShowing(mainWindow), false, 'sidebar is not showing in main window');
  assert.equal(isSidebarShowing(newWindow2), false, 'sidebar is not showing in second window');
  assert.equal(isSidebarShowing(newWindow), true, 'sidebar is showing in new window');

  assert.ok(isFocused(newWindow2), 'main window is still focused');

  yield sidebar.hide(newWindow);

  assert.equal(isFocused(newWindow2), true, 'second window is still focused');
  assert.equal(isSidebarShowing(mainWindow), false, 'sidebar is not showing in main window');
  assert.equal(isSidebarShowing(newWindow2), false, 'sidebar is not showing in second window');
  assert.equal(isSidebarShowing(newWindow), false, 'sidebar is not showing in new window');

  sidebar.destroy();
}

exports.testShowHideSDKWindowArg = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');

  let testName = 'testShowHideSDKWindowArg';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,' + testName
  });

  let mainWindow = getMostRecentBrowserWindow();
  let newWindow = yield open().then(focus);
  let newSDKWindow = modelFor(newWindow);

  yield focus(mainWindow);

  yield sidebar.show(newSDKWindow);

  assert.pass('the sidebar was shown');
  assert.ok(!isSidebarShowing(mainWindow), 'sidebar is not showing in main window');
  assert.ok(isSidebarShowing(newWindow), 'sidebar is showing in new window');

  assert.ok(isFocused(mainWindow), 'main window is still focused');

  yield sidebar.hide(newSDKWindow);

  assert.ok(isFocused(mainWindow), 'main window is still focused');
  assert.ok(!isSidebarShowing(mainWindow), 'sidebar is not showing in main window');
  assert.ok(!isSidebarShowing(newWindow), 'sidebar is not showing in new window');
  sidebar.destroy();
}

before(exports, (name, assert) => {
  assert.equal(isSidebarShowing(), false, 'no sidebar is showing');
});

after(exports, function*(name, assert) {
  assert.pass("Cleaning new windows and tabs");
  yield cleanUI();
  assert.equal(isSidebarShowing(), false, 'no sidebar is showing');
});

require('sdk/test').run(exports);
