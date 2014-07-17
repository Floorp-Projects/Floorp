/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
'use strict';

const { Loader, LoaderWithHookedConsole } = require("sdk/test/loader");
const { browserWindows } = require('sdk/windows');
const tabs = require('sdk/tabs');
const { isPrivate } = require('sdk/private-browsing');
const { openDialog } = require('sdk/window/utils');
const { isWindowPrivate } = require('sdk/window/utils');
const { setTimeout } = require('sdk/timers');
const { openWebpage } = require('./private-browsing/helper');
const { isTabPBSupported, isWindowPBSupported } = require('sdk/private-browsing/utils');
const app = require("sdk/system/xul-app");

const URL = 'data:text/html;charset=utf-8,<html><head><title>#title#</title></head></html>';

// TEST: tab count
exports.testTabCounts = function(assert, done) {
  tabs.open({
    url: 'about:blank',
    onReady: function(tab) {
      let count1 = 0,
          count2 = 0;
      for each(let window in browserWindows) {
        count1 += window.tabs.length;
        for each(let tab in window.tabs) {
          count2 += 1;
        }
      }

      assert.ok(tabs.length > 1, 'tab count is > 1');
      assert.equal(count1, tabs.length, 'tab count by length is correct');
      assert.equal(count2, tabs.length, 'tab count by iteration is correct');

      // end test
      tab.close(done);
    }
  });
};

exports.testTabRelativePath = function(assert, done) {
  const { merge } = require("sdk/util/object");
  const self = require("sdk/self");

  let loader = Loader(module, null, null, {
    modules: {
      "sdk/self": merge({}, self, {
        data: merge({}, self.data, require("./fixtures"))
      })
    }
  });

  let tabs = loader.require("sdk/tabs");

  tabs.open({
    url: "./test.html",
    onReady: (tab) => {
      assert.equal(tab.title, "foo",
        "tab opened a document with relative path");

      tab.attach({
        contentScriptFile: "./test-contentScriptFile.js",
        onMessage: (message) => {
          assert.equal(message, "msg from contentScriptFile",
            "Tab attach a contentScriptFile with relative path worked");

          tab.close(done);
        }
      });
    }
  });
};

// TEST: tabs.activeTab getter
exports.testActiveTab_getter = function(assert, done) {
  let evtCount = 0;
  let activeTab = null;

  function endTest(type, tab) {
    if (type == 'activate') {
      assert.strictEqual(tabs.activeTab, tab, 'the active tab is the opened tab');
      activeTab = tabs.activeTab;
    }
    else {
      assert.equal(tab.url, url, 'the opened tab has the correct url');
    }

    if (++evtCount != 2)
      return;

    assert.strictEqual(activeTab, tab, 'the active tab is the ready tab');
    assert.strictEqual(tabs.activeTab, tab, 'the active tab is the ready tab');

    tab.close(done);
  }

  let url = URL.replace("#title#", "testActiveTab_getter");
  tabs.open({
    url: url,
    onReady: endTest.bind(null, 'ready'),
    onActivate: endTest.bind(null, 'activate')
  });
};

// TEST: tab.activate()
exports.testActiveTab_setter = function(assert, done) {
  let url = URL.replace("#title#", "testActiveTab_setter");
  let tab1URL = URL.replace("#title#", "tab1");

  tabs.open({
    url: tab1URL,
    onReady: function(activeTab) {
      let activeTabURL = tabs.activeTab.url;

      tabs.open({
        url: url,
        inBackground: true,
        onReady: function onReady(tab) {
          assert.equal(tabs.activeTab.url, activeTabURL, "activeTab url has not changed");
          assert.equal(tab.url, url, "url of new background tab matches");

          tab.once('activate', function onActivate(eventTab) {
            assert.equal(tabs.activeTab.url, url, "url after activeTab setter matches");
            assert.equal(eventTab, tab, "event argument is the activated tab");
            assert.equal(eventTab, tabs.activeTab, "the tab is the active one");

            activeTab.close(function() {
              tab.close(done);
            });
          });

          tab.activate();
        }
      });
    }
  });
};

// TEST: tab.close()
exports.testTabClose_alt = function(assert, done) {
  let url = URL.replace('#title#', 'TabClose_alt');
  let tab1URL = URL.replace('#title#', 'tab1');

  tabs.open({
    url: tab1URL,
    onReady: function(tab1) {
      // make sure that our tab is not active first
      assert.notEqual(tabs.activeTab.url, url, "tab is not the active tab");

      tabs.open({
        url: url,
        onReady: function(tab) {
          assert.equal(tab.url, url, "tab is now the active tab");
          assert.equal(tabs.activeTab.url, url, "tab is now the active tab");

          // another tab should be activated on close
          tabs.once('activate', function() {
            assert.notEqual(tabs.activeTab.url, url, "tab is no longer the active tab");

            // end test
            tab1.close(done);
          });

          tab.close();
        }
      });
    }
  });
};

exports.testAttachOnOpen_alt = function (assert, done) {
  // Take care that attach has to be called on tab ready and not on tab open.
  tabs.open({
    url: "data:text/html;charset=utf-8,foobar",
    onOpen: function (tab) {
      let worker = tab.attach({
        contentScript: 'self.postMessage(document.location.href); ',
        onMessage: function (msg) {
          assert.equal(msg, "about:blank",
            "Worker document url is about:blank on open");
          worker.destroy();
          tab.close(done);
        }
      });
    }
  });
};

exports.testAttachOnMultipleDocuments_alt = function (assert, done) {
  // Example of attach that process multiple tab documents
  let firstLocation = "data:text/html;charset=utf-8,foobar";
  let secondLocation = "data:text/html;charset=utf-8,bar";
  let thirdLocation = "data:text/html;charset=utf-8,fox";
  let onReadyCount = 0;
  let worker1 = null;
  let worker2 = null;
  let detachEventCount = 0;

  tabs.open({
    url: firstLocation,
    onReady: function (tab) {
      onReadyCount++;
      if (onReadyCount == 1) {
        worker1 = tab.attach({
          contentScript: 'self.on("message", ' +
                         '  function () self.postMessage(document.location.href)' +
                         ');',
          onMessage: function (msg) {
            assert.equal(msg, firstLocation,
                             "Worker url is equal to the 1st document");
            tab.url = secondLocation;
          },
          onDetach: function () {
            detachEventCount++;
            assert.pass("Got worker1 detach event");
            assert.throws(function () {
                worker1.postMessage("ex-1");
              },
              /Couldn't find the worker/,
              "postMessage throw because worker1 is destroyed");
            checkEnd();
          }
        });
        worker1.postMessage("new-doc-1");
      }
      else if (onReadyCount == 2) {
        worker2 = tab.attach({
          contentScript: 'self.on("message", ' +
                         '  function () self.postMessage(document.location.href)' +
                         ');',
          onMessage: function (msg) {
            assert.equal(msg, secondLocation,
                             "Worker url is equal to the 2nd document");
            tab.url = thirdLocation;
          },
          onDetach: function () {
            detachEventCount++;
            assert.pass("Got worker2 detach event");
            assert.throws(function () {
                worker2.postMessage("ex-2");
              },
              /Couldn't find the worker/,
              "postMessage throw because worker2 is destroyed");
            checkEnd(tab);
          }
        });
        worker2.postMessage("new-doc-2");
      }
      else if (onReadyCount == 3) {
        tab.close();
      }
    }
  });

  function checkEnd(tab) {
    if (detachEventCount != 2)
      return;

    assert.pass("Got all detach events");

    done();
  }
};

exports.testAttachWrappers_alt = function (assert, done) {
  // Check that content script has access to wrapped values by default

  let document = "data:text/html;charset=utf-8,<script>var globalJSVar = true; " +
                 "                       document.getElementById = 3;</script>";
  let count = 0;

  tabs.open({
    url: document,
    onReady: function (tab) {
      let worker = tab.attach({
        contentScript: 'try {' +
                       '  self.postMessage(!("globalJSVar" in window));' +
                       '  self.postMessage(typeof window.globalJSVar == "undefined");' +
                       '} catch(e) {' +
                       '  self.postMessage(e.message);' +
                       '}',
        onMessage: function (msg) {
          assert.equal(msg, true, "Worker has wrapped objects ("+count+")");
          if (count++ == 1)
            tab.close(function() done());
        }
      });
    }
  });
};

// TEST: activeWindow getter and activeTab getter on tab 'activate' event
exports.testActiveWindowActiveTabOnActivate_alt = function(assert, done) {

  let activateCount = 0;
  let newTabs = [];
  let tabs = browserWindows.activeWindow.tabs;

  tabs.on('activate', function onActivate(tab) {
    assert.equal(tabs.activeTab, tab,
                    "the active window's active tab is the tab provided");

    if (++activateCount == 2) {
      tabs.removeListener('activate', onActivate);

      newTabs.forEach(function(tab) {
        tab.close(function() {
          if (--activateCount == 0) {
            done();
          }
        });
      });
    }
    else if (activateCount > 2) {
      assert.fail("activateCount is greater than 2 for some reason..");
    }
  });

  tabs.open({
    url: URL.replace("#title#", "tabs.open1"),
    onOpen: function(tab) newTabs.push(tab)
  });
  tabs.open({
    url: URL.replace("#title#", "tabs.open2"),
    onOpen: function(tab) newTabs.push(tab)
  });
};

// TEST: tab properties
exports.testTabContentTypeAndReload = function(assert, done) {

  let url = "data:text/html;charset=utf-8,<html><head><title>foo</title></head><body>foo</body></html>";
  let urlXML = "data:text/xml;charset=utf-8,<foo>bar</foo>";
  tabs.open({
    url: url,
    onReady: function(tab) {
      if (tab.url === url) {
        assert.equal(tab.contentType, "text/html");
        tab.url = urlXML;
      }
      else {
        assert.equal(tab.contentType, "text/xml");
        tab.close(done);
      }
    }
  });
};

// test that it isn't possible to open a private tab without the private permission
exports.testTabOpenPrivate = function(assert, done) {

  let url = 'about:blank';
  tabs.open({
    url: url,
    isPrivate: true,
    onReady: function(tab) {
      assert.equal(tab.url, url, 'opened correct tab');
      assert.equal(isPrivate(tab), false, 'private tabs are not supported by default');

      tab.close(done);
    }
  });
}

// We need permission flag in order to see private window's tabs
exports.testPrivateAreNotListed = function (assert, done) {
  let originalTabCount = tabs.length;

  let page = openWebpage("about:blank", true);
  if (!page) {
    assert.pass("Private browsing isn't supported in this release");
    return;
  }

  page.ready.then(function (win) {
    if (isTabPBSupported || isWindowPBSupported) {
      assert.ok(isWindowPrivate(win), "the window is private");
      assert.equal(tabs.length, originalTabCount,
                       'but the tab is *not* visible in tabs list');
    }
    else {
      assert.ok(!isWindowPrivate(win), "the window isn't private");
      assert.equal(tabs.length, originalTabCount + 1,
                       'so that the tab is visible is tabs list');
    }
    page.close().then(done);
  });
}

// If we close the tab while being in `onOpen` listener,
// we end up synchronously consuming TabOpen, closing the tab and still
// synchronously consuming the related TabClose event before the second
// loader have a change to process the first TabOpen event!
exports.testImmediateClosing = function (assert, done) {
  let tabURL = 'data:text/html,foo';

  let { loader, messages } = LoaderWithHookedConsole(module, onMessage);
  let concurrentTabs = loader.require("sdk/tabs");
  concurrentTabs.on("open", function (tab) {
    // On Firefox, It shouldn't receive such event as the other loader will just
    // open and destroy the tab without giving a chance to other loader to even
    // know about the existance of this tab.
    if (app.is("Firefox")) {
      assert.fail("Concurrent loader received a tabs `open` event");
    }
    else {
      // On mobile, we can still receive an open event,
      // but not the related ready event
      tab.on("ready", function () {
        assert.fail("Concurrent loader received a tabs `ready` event");
      });
    }
  });
  function onMessage(type, msg) {
    assert.fail("Unexpected mesage on concurrent loader: " + msg);
  }

  tabs.open({
    url: tabURL,
    onOpen: function(tab) {
      tab.close(function () {
        assert.pass("Tab succesfully removed");
        // Let a chance to the concurrent loader to receive a TabOpen event
        // on the next event loop turn
        setTimeout(function () {
          loader.unload();
          done();
        }, 0);
      });
    }
  });
}

// TEST: tab.reload()
exports.testTabReload = function(assert, done) {

  let url = "data:text/html;charset=utf-8,<!doctype%20html><title></title>";

  tabs.open({
    url: url,
    onReady: function onReady(tab) {
      tab.removeListener('ready', onReady);

      tab.once(
        'ready',
        function onReload() {
          assert.pass("the tab was loaded again");
          assert.equal(tab.url, url, "the tab has the same URL");

          tab.close(function() done());
        }
      );

      tab.reload();
    }
  });
};

exports.testOnPageShowEvent = function (assert, done) {
  let events = [];
  let firstUrl = 'data:text/html;charset=utf-8,First';
  let secondUrl = 'data:text/html;charset=utf-8,Second';

  let counter = 0;
  function onPageShow (tab, persisted) {
    events.push('pageshow');
    counter++;
    if (counter === 1) {
      assert.equal(persisted, false, 'page should not be cached on initial load');
      tab.url = secondUrl;
    }
    else if (counter === 2) {
      assert.equal(persisted, false, 'second test page should not be cached either');
      tab.attach({
        contentScript: 'setTimeout(function () { window.history.back(); }, 0)'
      });
    }
    else {
      assert.equal(persisted, true, 'when we get back to the fist page, it has to' +
                             'come from cache');
      tabs.removeListener('pageshow', onPageShow);
      tabs.removeListener('open', onOpen);
      tabs.removeListener('ready', onReady);
      tab.close(() => {
        ['open', 'ready', 'pageshow', 'ready',
            'pageshow', 'pageshow'].map((type, i) => {
          assert.equal(type, events[i], 'correct ordering of events');
        });
        done()
      });
    }
  }

  function onOpen () events.push('open');
  function onReady () events.push('ready');

  tabs.on('pageshow', onPageShow);
  tabs.on('open', onOpen);
  tabs.on('ready', onReady);
  tabs.open({
    url: firstUrl
  });
};

exports.testOnPageShowEventDeclarative = function (assert, done) {
  let events = [];
  let firstUrl = 'data:text/html;charset=utf-8,First';
  let secondUrl = 'data:text/html;charset=utf-8,Second';

  let counter = 0;
  function onPageShow (tab, persisted) {
    events.push('pageshow');
    counter++;
    if (counter === 1) {
      assert.equal(persisted, false, 'page should not be cached on initial load');
      tab.url = secondUrl;
    }
    else if (counter === 2) {
      assert.equal(persisted, false, 'second test page should not be cached either');
      tab.attach({
        contentScript: 'setTimeout(function () { window.history.back(); }, 0)'
      });
    }
    else {
      assert.equal(persisted, true, 'when we get back to the fist page, it has to' +
                             'come from cache');
      tabs.removeListener('pageshow', onPageShow);
      tabs.removeListener('open', onOpen);
      tabs.removeListener('ready', onReady);
      tab.close(() => {
        ['open', 'ready', 'pageshow', 'ready',
            'pageshow', 'pageshow'].map((type, i) => {
          assert.equal(type, events[i], 'correct ordering of events');
        });
        done()
      });
    }
  }

  function onOpen () events.push('open');
  function onReady () events.push('ready');

  tabs.open({
    url: firstUrl,
    onPageShow: onPageShow,
    onOpen: onOpen,
    onReady: onReady
  });
};

require('sdk/test').run(exports);
