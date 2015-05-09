/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const tabs = require("sdk/tabs");
const windowUtils = require("sdk/deprecated/window-utils");
const { getTabForWindow } = require('sdk/tabs/helpers');
const windows = require("sdk/windows").browserWindows;
const app = require("sdk/system/xul-app");
const { viewFor } = require("sdk/view/core");
const { modelFor } = require("sdk/model/core");
const { getTabId, isTab } = require("sdk/tabs/utils");
const { defer } = require("sdk/lang/functional");

// The primary test tab
var primaryTab;

// We have an auxiliary tab to test background tabs.
var auxTab;

// The window for the outer iframe in the primary test page
var iframeWin;

function tabExistenceInTabs(assert, found, tab, tabs) {
  let tabFound = false;

  for each (let t in tabs) {
    assert.notEqual(t.title, undefined, 'tab title is not undefined');
    assert.notEqual(t.url, undefined, 'tab url is not undefined');
    assert.notEqual(t.index, undefined, 'tab index is not undefined');

    if (t === tab) {
      tabFound = true;
      break;
    }
  }

  // check for the tab's existence
  if (found)
    assert.ok(tabFound, 'the tab was found as expected');
  else
    assert.ok(!tabFound, 'the tab was not found as expected');
}

exports["test GetTabForWindow"] = function(assert, done) {

  assert.equal(getTabForWindow(windowUtils.activeWindow), null,
    "getTabForWindow return null on topwindow");
  assert.equal(getTabForWindow(windowUtils.activeBrowserWindow), null,
    "getTabForWindow return null on topwindow");

  let subSubDocument = encodeURIComponent(
    'Sub iframe<br/>'+
    '<iframe id="sub-sub-iframe" src="data:text/html;charset=utf-8,SubSubIframe" />');
  let subDocument = encodeURIComponent(
    'Iframe<br/>'+
    '<iframe id="sub-iframe" src="data:text/html;charset=utf-8,'+subSubDocument+'" />');
  let url = 'data:text/html;charset=utf-8,' + encodeURIComponent(
    'Content<br/><iframe id="iframe" src="data:text/html;charset=utf-8,'+subDocument+'" />');

  // Open up a new tab in the background.
  //
  // This lets us test whether GetTabForWindow works even when the tab in
  // question is not active.
  tabs.open({
    inBackground: true,
    url: "about:mozilla",
    onReady: function(tab) { auxTab = tab; step2(url, assert);},
    onActivate: function(tab) { step3(assert, done); }
  });
};

function step2(url, assert) {

  tabs.open({
    url: url,
    onReady: function(tab) {
      primaryTab = tab;
      let window = windowUtils.activeBrowserWindow.content;

      let matchedTab = getTabForWindow(window);
      assert.equal(matchedTab, tab,
        "We are able to find the tab with his content window object");

      let timer = require("sdk/timers");
      function waitForFrames() {
        let iframe = window.document.getElementById("iframe");
        if (!iframe) {
          timer.setTimeout(waitForFrames, 100);
          return;
        }
        iframeWin = iframe.contentWindow;
        let subIframe = iframeWin.document.getElementById("sub-iframe");
        if (!subIframe) {
          timer.setTimeout(waitForFrames, 100);
          return;
        }
        let subIframeWin = subIframe.contentWindow;
        let subSubIframe = subIframeWin.document.getElementById("sub-sub-iframe");
        if (!subSubIframe) {
          timer.setTimeout(waitForFrames, 100);
          return;
        }
        let subSubIframeWin = subSubIframe.contentWindow;

        matchedTab = getTabForWindow(iframeWin);
        assert.equal(matchedTab, tab,
          "We are able to find the tab with an iframe window object");

        matchedTab = getTabForWindow(subIframeWin);
        assert.equal(matchedTab, tab,
          "We are able to find the tab with a sub-iframe window object");

        matchedTab = getTabForWindow(subSubIframeWin);
        assert.equal(matchedTab, tab,
          "We are able to find the tab with a sub-sub-iframe window object");

        // Put our primary tab in the background and test again.
        // The onActivate listener will take us to step3.
        auxTab.activate();
      }
      waitForFrames();
    }
  });
}

function step3(assert, done) {

  let matchedTab = getTabForWindow(iframeWin);
  assert.equal(matchedTab, primaryTab,
    "We get the correct tab even when it's in the background");

  primaryTab.close(function () {
      auxTab.close(function () { done();});
    });
}

exports.testBehaviorOnCloseAfterReady = function(assert, done) {
  tabs.open({
    url: "about:mozilla",
    onReady: function(tab) {
      assert.equal(tab.url, "about:mozilla", "Tab has the expected url");
      // if another test ends before closing a tab then index != 1 here
      assert.ok(tab.index >= 1, "Tab has the expected index, a value greater than 0");

      let testTabFound = tabExistenceInTabs.bind(null, assert, true, tab);
      let testTabNotFound = tabExistenceInTabs.bind(null, assert, false, tab);

      // tab in require("sdk/tabs") ?
      testTabFound(tabs);
      // tab was found in require("sdk/windows").windows.activeWindow.tabs ?
      let activeWindowTabs = windows.activeWindow.tabs;
      testTabFound(activeWindowTabs);

      tab.close(function () {
        assert.equal(tab.url, undefined,
                     "After being closed, tab attributes are undefined (url)");
        assert.equal(tab.index, undefined,
                     "After being closed, tab attributes are undefined (index)");

        tab.destroy();

        testTabNotFound(tabs);
        testTabNotFound(windows.activeWindow.tabs);
        testTabNotFound(activeWindowTabs);

        // Ensure that we can call destroy multiple times without throwing
        tab.destroy();

        testTabNotFound(tabs);
        testTabNotFound(windows.activeWindow.tabs);
        testTabNotFound(activeWindowTabs);

        done();
      });
    }
  });
};

exports.testBug844492 = function(assert, done) {
  const activeWindowTabs = windows.activeWindow.tabs;
  let openedTabs = 0;

  tabs.on('open', function onOpenTab(tab) {
    openedTabs++;

    let testTabFound = tabExistenceInTabs.bind(null, assert, true, tab);
    let testTabNotFound = tabExistenceInTabs.bind(null, assert, false, tab);

    testTabFound(tabs);
    testTabFound(windows.activeWindow.tabs);
    testTabFound(activeWindowTabs);

    tab.close();

    testTabNotFound(tabs);
    testTabNotFound(windows.activeWindow.tabs);
    testTabNotFound(activeWindowTabs);

    if (openedTabs >= 2) {
      tabs.removeListener('open', onOpenTab);
      done();
    }
  });

  tabs.open({ url: 'about:mozilla' });
  tabs.open({ url: 'about:mozilla' });
};

exports.testBehaviorOnCloseAfterOpen = function(assert, done) {
  tabs.open({
    url: "about:mozilla",
    onOpen: function(tab) {
      assert.notEqual(tab.url, undefined, "Tab has a url");
      assert.ok(tab.index >= 1, "Tab has the expected index");

      let testTabFound = tabExistenceInTabs.bind(null, assert, true, tab);
      let testTabNotFound = tabExistenceInTabs.bind(null, assert, false, tab);

      // tab in require("sdk/tabs") ?
      testTabFound(tabs);
      // tab was found in require("sdk/windows").windows.activeWindow.tabs ?
      let activeWindowTabs = windows.activeWindow.tabs;
      testTabFound(activeWindowTabs);

      tab.close(function () {
        assert.equal(tab.url, undefined,
                     "After being closed, tab attributes are undefined (url)");
        assert.equal(tab.index, undefined,
                     "After being closed, tab attributes are undefined (index)");

        tab.destroy();

        if (app.is("Firefox")) {
          // Ensure that we can call destroy multiple times without throwing;
          // Fennec doesn't use this internal utility
          tab.destroy();
          tab.destroy();
        }

        testTabNotFound(tabs);
        testTabNotFound(windows.activeWindow.tabs);
        testTabNotFound(activeWindowTabs);

        // Ensure that we can call destroy multiple times without throwing
        tab.destroy();

        testTabNotFound(tabs);
        testTabNotFound(windows.activeWindow.tabs);
        testTabNotFound(activeWindowTabs);

        done();
      });
    }
  });
};

exports["test viewFor(tab)"] = (assert, done) => {
  // Note we defer handlers as length collection is updated after
  // handler is invoked, so if test is finished before counnts are
  // updated wrong length will show up in followup tests.
  tabs.once("open", defer(tab => {
    const view = viewFor(tab);
    assert.ok(view, "view is returned");
    assert.equal(getTabId(view), tab.id, "tab has a same id");

    tab.close(defer(done));
  }));

  tabs.open({ url: "about:mozilla" });
};


exports["test modelFor(xulTab)"] = (assert, done) => {
  tabs.open({
    url: "about:mozilla",
    onReady: tab => {
      const view = viewFor(tab);
      assert.ok(view, "view is returned");
      assert.ok(isTab(view), "view is underlaying tab");
      assert.equal(getTabId(view), tab.id, "tab has a same id");
      assert.equal(modelFor(view), tab, "modelFor(view) is SDK tab");

      tab.close(defer(done));
    }
  });
};

exports["test tab.readyState"] = (assert, done) => {
  tabs.open({
    url: "data:text/html;charset=utf-8,test_readyState",
    onOpen: (tab) => {
      assert.notEqual(["uninitialized", "loading"].indexOf(tab.readyState), -1,
        "tab is either uninitialized or loading when onOpen");
    },
    onReady: (tab) => {
      assert.notEqual(["interactive", "complete"].indexOf(tab.readyState), -1,
        "tab is either interactive or complete when onReady");
    },
    onLoad: (tab) => {
      assert.equal(tab.readyState, "complete", "tab is complete onLoad");
      tab.close(defer(done));
    }
  });
}

require("sdk/test").run(module.exports);
