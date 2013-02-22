/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


const tabs = require("sdk/tabs"); // From addon-kit
const windowUtils = require("sdk/deprecated/window-utils");
const { getTabForWindow } = require('sdk/tabs/helpers');

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
        // Ensure that we can call destroy multiple times without throwing
        tab.destroy();
        tab.destroy();

        done();
      });
    }
  });
};

if (require("sdk/system/xul-app").is("Fennec")) {
  module.exports = {
    "test Unsupported Test": function UnsupportedTest (assert) {
        assert.pass(
          "Skipping this test until Fennec support is implemented." +
          "See Bug 809362");
    }
  }
}

require("test").run(exports);
