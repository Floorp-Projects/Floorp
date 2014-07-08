/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const tabs = require("sdk/tabs"); // From addon-kit
const windowUtils = require("sdk/deprecated/window-utils");
const { getTabForWindow } = require('sdk/tabs/helpers');
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

exports["test behavior on close"] = function(assert, done) {

  tabs.open({
    url: "about:mozilla",
    onReady: function(tab) {
      assert.equal(tab.url, "about:mozilla", "Tab has the expected url");
      // if another test ends before closing a tab then index != 1 here
      assert.ok(tab.index >= 1, "Tab has the expected index, a value greater than 0");
      tab.close(function () {
        assert.equal(tab.url, undefined,
                     "After being closed, tab attributes are undefined (url)");
        assert.equal(tab.index, undefined,
                     "After being closed, tab attributes are undefined (index)");
        if (app.is("Firefox")) {
          // Ensure that we can call destroy multiple times without throwing;
          // Fennec doesn't use this internal utility
          tab.destroy();
          tab.destroy();
        }

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
      assert.equal(tab.readyState, "uninitialized",
        "tab is 'uninitialized' when opened");
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

// require("sdk/test").run(exports);
