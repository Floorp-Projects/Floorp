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
require('sdk/windows');

const { BUILTIN_SIDEBAR_MENUITEMS, isSidebarShowing,
        getSidebarMenuitems, getExtraSidebarMenuitems, makeID, simulateCommand,
        simulateClick, isChecked } = require('./sidebar/utils');

exports.testSidebarBasicLifeCycle = function(assert, done) {
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
  sidebar.on('show', function() {
    assert.pass('the show event was fired');
    assert.equal(isSidebarShowing(window), true, 'sidebar is not showing 3');
    assert.equal(isShowing(sidebar), true, 'the sidebar is showing');
    assert.ok(isChecked(ele), 'the sidebar is displayed');

    sidebar.once('hide', function() {
      assert.pass('the hide event was fired');
      assert.ok(!isChecked(ele), 'the sidebar menuitem is not checked');
      assert.equal(isShowing(sidebar), false, 'the sidebar is not showing');
      assert.equal(isSidebarShowing(window), false, 'the sidebar elemnt is hidden');

      sidebar.once('detach', function() {
        // calling destroy twice should not matter
        sidebar.destroy();
        sidebar.destroy();

        let sidebarMI = getSidebarMenuitems();
        for (let mi of sidebarMI) {
          assert.ok(BUILTIN_SIDEBAR_MENUITEMS.indexOf(mi.getAttribute('id')) >= 0, 'the menuitem is for a built-in sidebar')
          assert.ok(!isChecked(mi), 'no sidebar menuitem is checked');
        }

        assert.ok(!window.document.getElementById(makeID(testName)), 'sidebar id DNE');
        assert.pass('calling destroy worked without error');

        done();
      });
    });

    sidebar.hide();
    assert.pass('hiding sidebar..');
  });

  sidebar.show();
  assert.pass('showing sidebar..');
}

exports.testSideBarIsInNewWindows = function(assert, done) {
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

  open().then(function(window) {
      let ele = window.document.getElementById(makeID(testName));
      assert.ok(ele, 'sidebar element was added');

      // calling destroy twice should not matter
      sidebar.destroy();
      sidebar.destroy();

      assert.ok(!window.document.getElementById(makeID(testName)), 'sidebar id DNE');
      assert.ok(!startWindow.document.getElementById(makeID(testName)), 'sidebar id DNE');

      close(window).then(done, assert.fail);
  })
}

exports.testSideBarIsShowingInNewWindows = function(assert, done) {
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
  sidebar.once('attach', function() {
    assert.pass('attach event fired');

    sidebar.once('show', function() {
      assert.pass('show event fired');

      sidebar.once('show', function() {
        let window = getMostRecentBrowserWindow();
        assert.notEqual(startWindow, window, 'window is new');

        let sb = window.document.getElementById('sidebar');
        if (sb && sb.docShell && sb.contentDocument && sb.contentDocument.getElementById('web-panels-browser')) {
          end();
        }
        else {
          sb.addEventListener('DOMWindowCreated', end, false);
        }

        function end() {
          sb.removeEventListener('DOMWindowCreated', end, false);
          let webPanelBrowser = sb.contentDocument.getElementById('web-panels-browser');

          let ele = window.document.getElementById(makeID(testName));

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

            setTimeout(function() {
              close(window).then(done, assert.fail);
            });
        }
      });

      startWindow.OpenBrowserWindow();
    });
  });

  show(sidebar);
  assert.pass('showing the sidebar');
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

exports.testAddonGlobalComplex = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testAddonGlobalComplex';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: data.url('test-sidebar-addon-global.html')
  });

  sidebar.on('attach', function(worker) {
    assert.pass('sidebar was attached');
    assert.ok(!!worker, 'attach event has worker');

    worker.port.once('Y', function(msg) {
      assert.equal(msg, '1', 'got event from worker');

      worker.port.on('X', function(msg) {
        assert.equal(msg, '123', 'the final message is correct');

        sidebar.destroy();

        done();
      });
      worker.port.emit('X', msg + '2');
    })
  });

  show(sidebar);
}

exports.testAddonReady = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testAddonReady';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: data.url('test-sidebar-addon-global.html'),
    onReady: function(worker) {
      assert.pass('sidebar was attached');
      assert.ok(!!worker, 'attach event has worker');

      worker.port.on('X', function(msg) {
        assert.equal(msg, '123', 'the final message is correct');

        sidebar.destroy();

        done();
      });

      worker.port.emit('X', '12');
    }
  });

  show(sidebar);
}

exports.testShowingOneSidebarAfterAnother = function(assert, done) {
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

  sidebar1.once('show', function() {
    testShowing(true, false, true);
    for (let mi of getExtraSidebarMenuitems(window)) {
      let menuitemID = mi.getAttribute('id').replace(/^jetpack-sidebar-/, '');
      assert.ok(IDs.indexOf(menuitemID) >= 0, 'the extra menuitem is for one of our test sidebars');
      assert.equal(isChecked(mi), menuitemID == sidebar1.id, 'the test sidebar menuitem has the correct checked value');
    }

    sidebar2.once('show', function() {
      testShowing(false, true, true);
      for (let mi of getExtraSidebarMenuitems(window)) {
        let menuitemID = mi.getAttribute('id').replace(/^jetpack-sidebar-/, '');
        assert.ok(IDs.indexOf(menuitemID) >= 0, 'the extra menuitem is for one of our test sidebars');
        assert.equal(isChecked(mi), menuitemID == sidebar2.id, 'the test sidebar menuitem has the correct checked value');
      }

      sidebar1.destroy();
      sidebar2.destroy();

      testShowing(false, false, false);

      done();
    });

    show(sidebar2);
    assert.pass('showing sidebar 2');
  })
  show(sidebar1);
  assert.pass('showing sidebar 1');
}

exports.testSidebarUnload = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testSidebarUnload';
  let loader = Loader(module);

  let window = getMostRecentBrowserWindow();

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
}

exports.testRelativeURL = function(assert, done) {
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

  sidebar.on('attach', function(worker) {
    assert.pass('sidebar was attached');
    assert.ok(!!worker, 'attach event has worker');

    worker.port.once('Y', function(msg) {
      assert.equal(msg, '1', 'got event from worker');

      worker.port.on('X', function(msg) {
        assert.equal(msg, '123', 'the final message is correct');

        sidebar.destroy();

        done();
      });
      worker.port.emit('X', msg + '2');
    })
  });

  sidebar.show();
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
exports.testDestroyEdgeCaseBug = function(assert, done) {
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

  open().then(focus).then(function(window2) {
    assert.equal(isPrivate(window2), false, 'the new window is not private');
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
    });
  });
}

exports.testClickingACheckedMenuitem = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  const testName = 'testClickingACheckedMenuitem';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,'+testName,
  });
  assert.pass('sidebar was created');

  open().then(focus).then(window => {
    return sidebar.show().then(_ => {
      assert.pass('the show callback works');

      sidebar.once('hide', _ => {
        assert.pass('clicking the menuitem after the sidebar has shown hides it.');
        sidebar.destroy();
        close(window).then(done, assert.fail);
      });

      let menuitem = window.document.getElementById(makeID(sidebar.id));
      simulateCommand(menuitem);
    });
  }).catch(assert.fail);
};

exports.testTitleSetter = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testTitleSetter';
  let { document } = getMostRecentBrowserWindow();

  let sidebar1 = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,'+testName,
  });

  assert.equal(sidebar1.title, testName, 'title getter works');

  sidebar1.show().then(function() {
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
    done();
  }, assert.fail);
}

exports.testURLSetter = function(assert, done) {
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

  windowPromise(window.OpenBrowserWindow(), 'load').then(function(window) {
    let { document } = window;
    assert.pass('new window was opened');

    sidebar1.show().then(function() {
      assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
      assert.ok(isChecked(document.getElementById(makeID(sidebar1.id))),
                   'the menuitem is checked');
      assert.ok(isSidebarShowing(window), 'the new window sidebar is showing');

      sidebar1.once('show', function() {
        assert.pass('setting the sidebar.url causes a show event');

        assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
        assert.ok(isSidebarShowing(window), 'the new window sidebar is still showing');

        assert.ok(isChecked(document.getElementById(makeID(sidebar1.id))),
                     'the menuitem is still checked');

        sidebar1.destroy();

        close(window).then(done);
      });

      sidebar1.url = (url + '1');

      assert.equal(sidebar1.url, (url + '1'), 'url getter works');
      assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
      assert.ok(isSidebarShowing(window), 'the new window sidebar is showing');
    }, assert.fail);
  }, assert.fail);
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

exports.testURLSetterToSameValueReloadsSidebar = function(assert, done) {
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

  windowPromise(window.OpenBrowserWindow(), 'load').then(function(window) {
    let { document } = window;
    assert.pass('new window was opened');

    sidebar1.show().then(function() {
      assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
      assert.ok(isChecked(document.getElementById(makeID(sidebar1.id))),
                   'the menuitem is checked');
      assert.ok(isSidebarShowing(window), 'the new window sidebar is showing');

      sidebar1.once('show', function() {
        assert.pass('setting the sidebar.url causes a show event');

        assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
        assert.ok(isSidebarShowing(window), 'the new window sidebar is still showing');

        assert.ok(isChecked(document.getElementById(makeID(sidebar1.id))),
                     'the menuitem is still checked');

        sidebar1.destroy();

        close(window).then(done);
      });

      sidebar1.url = url;

      assert.equal(sidebar1.url, url, 'url getter works');
      assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
      assert.ok(isSidebarShowing(window), 'the new window sidebar is showing');
    }, assert.fail);
  }, assert.fail);
}

exports.testShowingInOneWindowDoesNotAffectOtherWindows = function(assert, done) {
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

  windowPromise(window1.OpenBrowserWindow(), 'load').then(function(window) {
    let { document } = window;
    assert.pass('new window was opened!');

    // waiting for show
    sidebar1.once('show', function() {
      // check state of the new window
      assert.equal(isShowing(sidebar1), true, 'the sidebar is showing');
      checkSidebarShowing(window, true);

      // check state of old window
      checkSidebarShowing(window1, false);

      // waiting for show using url setter
      sidebar1.once('show', function() {
        assert.pass('setting the sidebar.url causes a new show event');

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

        close(window).then(done);
      });

      assert.pass('setting sidebar1.url');
      sidebar1.url += '1';
      assert.pass('set sidebar1.url');
    });

    sidebar1.show();
  }, assert.fail);
}

exports.testHidingAHiddenSidebarRejects = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testHidingAHiddenSidebarRejects';
  let url = 'data:text/html;charset=utf-8,'+testName;
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: url
  });

  sidebar.hide().then(assert.fail, assert.pass).then(function() {
    sidebar.destroy();
    done();
  }, assert.fail);
}

exports.testGCdSidebarsOnUnload = function(assert, done) {
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

  sidebar.show().then(function() {
    sidebar = null;

    assert.equal(isSidebarShowing(window), true, 'the sidebar is showing');

    let menuitemID = makeID(testName);

    assert.ok(!!window.document.getElementById(menuitemID), 'the menuitem was found');

    Cu.schedulePreciseGC(function() {
      loader.unload();

      assert.equal(isSidebarShowing(window), false, 'the sidebar is not showing after unload');
      assert.ok(!window.document.getElementById(menuitemID), 'the menuitem was removed');

      done();
    })
  }, assert.fail).then(null, assert.fail);
}

exports.testGCdShowingSidebarsOnUnload = function(assert, done) {
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

  sidebar.on('show', function() {
    sidebar = null;

    assert.equal(isSidebarShowing(window), true, 'the sidebar is showing');

    let menuitemID = makeID(testName);

    assert.ok(!!window.document.getElementById(menuitemID), 'the menuitem was found');

    Cu.schedulePreciseGC(function() {
      assert.equal(isSidebarShowing(window), true, 'the sidebar is still showing after gc');
      assert.ok(!!window.document.getElementById(menuitemID), 'the menuitem was found after gc');

      loader.unload();

      assert.equal(isSidebarShowing(window), false, 'the sidebar is not showing after unload');
      assert.ok(!window.document.getElementById(menuitemID), 'the menuitem was removed');

      done();
    })
  });

  sidebar.show();
}

exports.testDetachEventOnWindowClose = function(assert, done) {
  const loader = Loader(module);
  const { Sidebar } = loader.require('sdk/ui/sidebar');
  const window = getMostRecentBrowserWindow();

  let testName = 'testDetachEventOnWindowClose';
  let url = 'data:text/html;charset=utf-8,' + testName;


  windowPromise(window.OpenBrowserWindow(), 'load').then(focus).then(function(window) {
    let sidebar = Sidebar({
      id: testName,
      title: testName,
      url: url,
      onAttach: function() {
        assert.pass('the attach event is fired');
        window.close();
      },
      onDetach: function() {
        assert.pass('the detach event is fired when the window showing it closes');
        loader.unload();
        done();
      }
    });

    sidebar.show();
  }).then(null, assert.fail);
}

exports.testHideEventOnWindowClose = function(assert, done) {
  const loader = Loader(module);
  const { Sidebar } = loader.require('sdk/ui/sidebar');
  const window = getMostRecentBrowserWindow();

  let testName = 'testDetachEventOnWindowClose';
  let url = 'data:text/html;charset=utf-8,' + testName;


  windowPromise(window.OpenBrowserWindow(), 'load').then(focus).then(function(window) {
    let sidebar = Sidebar({
      id: testName,
      title: testName,
      url: url,
      onAttach: function() {
        assert.pass('the attach event is fired');
        window.close();
      },
      onHide: function() {
        assert.pass('the hide event is fired when the window showing it closes');
        loader.unload();
        done();
      }
    });

    sidebar.show();
  }).then(null, assert.fail);
}

exports.testGCdHiddenSidebarsOnUnload = function(assert, done) {
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

  Cu.schedulePreciseGC(function() {
    assert.ok(!!window.document.getElementById(menuitemID), 'the menuitem was found after gc');

    loader.unload();

    assert.ok(!window.document.getElementById(menuitemID), 'the menuitem was removed');

    done();
  });
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


exports.testSidebarLeakCheckDestroyAfterAttach = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testSidebarLeakCheckDestroyAfterAttach';
  let window = getMostRecentBrowserWindow();
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,'+testName
  });

  sidebar.on('attach', function() {
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
    panelBrowser.contentWindow.addEventListener('unload', function onUnload() {
      panelBrowser.contentWindow.removeEventListener('unload', onUnload, false);
      // wait a tick..
      setTimeout(function() {
        assert.pass('the sidebar web panel was unloaded properly');
        done();
      })
    }, false);

    sidebar.destroy();
  });

  assert.pass('showing the sidebar');
  sidebar.show();
}

exports.testSidebarLeakCheckUnloadAfterAttach = function(assert, done) {
  const loader = Loader(module);
  const { Sidebar } = loader.require('sdk/ui/sidebar');
  let testName = 'testSidebarLeakCheckUnloadAfterAttach';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,'+testName
  });

  open().then(focus).then(window => {
    sidebar.on('attach', function() {
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
      panelBrowser.contentWindow.addEventListener('unload', function onUnload() {
        panelBrowser.contentWindow.removeEventListener('unload', onUnload, false);
        assert.pass('the sidebar web panel was unloaded properly');
        close(window).then(done).catch(assert.fail);
      }, false);

      loader.unload();
    });

    assert.pass('showing the sidebar');
    sidebar.show();
  }).catch(assert.fail);
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

exports.testShowToOpenXToClose = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testShowToOpenXToClose';

  let title = testName;
  let url = 'data:text/html;charset=utf-8,' + testName;
  let window = getMostRecentBrowserWindow();

  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: url,
    onShow: function() {
      assert.ok(isChecked(menuitem), 'menuitem is checked');

      let closeButton = window.document.querySelector('#sidebar-header > toolbarbutton.close-icon');
      simulateCommand(closeButton);
    },
    onHide: function() {
      assert.ok(!isChecked(menuitem), 'menuitem is not checked');

      sidebar.destroy();
      done();
    }
  });

  let menuitem = window.document.getElementById(makeID(sidebar.id));

  assert.ok(!isChecked(menuitem), 'menuitem is not checked');

  sidebar.show();
}

exports.testShowToOpenMenuitemToClose = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testShowToOpenMenuitemToClose';

  let title = testName;
  let url = 'data:text/html;charset=utf-8,' + testName;
  let window = getMostRecentBrowserWindow();

  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: url,
    onShow: function() {
      assert.ok(isChecked(menuitem), 'menuitem is checked');

      simulateCommand(menuitem);
    },
    onHide: function() {
      assert.ok(!isChecked(menuitem), 'menuitem is not checked');

      sidebar.destroy();
      done();
    }
  });

  let menuitem = window.document.getElementById(makeID(sidebar.id));

  assert.ok(!isChecked(menuitem), 'menuitem is not checked');

  sidebar.show();
}

exports.testDestroyWhileNonBrowserWindowIsOpen = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testDestroyWhileNonBrowserWindowIsOpen';
  let url = 'data:text/html;charset=utf-8,' + testName;

  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: url
  });

  open('chrome://browser/content/preferences/preferences.xul').then(function(window) {
    try {
      sidebar.show();
      assert.equal(isSidebarShowing(getMostRecentBrowserWindow()), true, 'the sidebar is showing');

      sidebar.destroy();

      assert.pass('sidebar was destroyed while a non browser window was open');
    }
    catch(e) {
      assert.fail(e);
    }

    return window;
  }).then(close).then(function() {
    assert.equal(isSidebarShowing(getMostRecentBrowserWindow()), false, 'the sidebar is not showing');
  }).then(done, assert.fail);
}

exports.testEventListeners = function(assert, done) {
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

  all([constructorOnShow.promise,
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
      onHide.promise]).then(function() {
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
      }).then(done, assert.fail);

  sidebar.show();
}

// For more information see Bug 920780
exports.testAttachDoesNotEmitWhenShown = function(assert, done) {
  const { Sidebar } = require('sdk/ui/sidebar');
  let testName = 'testAttachDoesNotEmitWhenShown';
  let count = 0;

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

      if (++count == 1) {
        setImmediate(function() {
          let shownFired = 0;
          let onShow = () => shownFired++;
          sidebar.on('show', onShow);

          sidebar.show()
          .then(() => assert.equal(shownFired, 0, 'shown should not be fired again when already showing from after attach'))
          .then(sidebar.hide.bind(sidebar))
          .then(sidebar.show.bind(sidebar))
          .then(() => assert.equal(shownFired, 1, 'shown was emitted when `show` called after being hidden'))
          .then(sidebar.show.bind(sidebar))
          .then(() => {
            assert.equal(shownFired, 1, 'shown was not emitted again if already being shown');
            sidebar.off('show', onShow);
            sidebar.destroy();
          }).catch(assert.fail).then(done);
        });
      }
    }
  });

  sidebar.show();
}

exports.testShowHideRawWindowArg = function*(assert) {
  const { Sidebar } = require('sdk/ui/sidebar');

  let testName = 'testShowHideRawWindowArg';
  let sidebar = Sidebar({
    id: testName,
    title: testName,
    url: 'data:text/html;charset=utf-8,' + testName
  });

  let mainWindow = getMostRecentBrowserWindow();
  let newWindow = yield open().then(focus);

  yield focus(mainWindow);

  yield sidebar.show(newWindow);

  assert.pass('the sidebar was shown');
  assert.ok(!isSidebarShowing(mainWindow), 'sidebar is not showing in main window');
  assert.ok(isSidebarShowing(newWindow), 'sidebar is showing in new window');

  assert.ok(isFocused(mainWindow), 'main window is still focused');

  yield sidebar.hide(newWindow);

  assert.ok(isFocused(mainWindow), 'main window is still focused');
  assert.ok(!isSidebarShowing(mainWindow), 'sidebar is not showing in main window');
  assert.ok(!isSidebarShowing(newWindow), 'sidebar is not showing in new window');
  sidebar.destroy();

  yield close(newWindow);
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

  yield close(newWindow);
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
