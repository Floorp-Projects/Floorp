/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  //initialization
  waitForExplicitFinish();

  let scriptLoader = Cc["@mozilla.org/moz/jssubscript-loader;1"].
                     getService(Ci.mozIJSSubScriptLoader);
  let ChromeUtils = {};
  scriptLoader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/ChromeUtils.js", ChromeUtils);

  function testOnWindow(aIsPrivate, aCallback) {
    whenNewWindowLoaded({private: aIsPrivate}, function(win) {
      executeSoon(function() aCallback(win));
    });
  }

  testOnWindow(false, function(aNormalWindow) {
    testOnWindow(true, function(aPrivateWindow) {
      // Open a tab in each window
      let normalTab = aNormalWindow.gBrowser.addTab("about:blank", {skipAnimation: true});
      let privateTab = aPrivateWindow.gBrowser.addTab("about:blank", {skipAnimation: true});

      let effect = ChromeUtils.synthesizeDrop(normalTab, privateTab,
        [[{type: TAB_DROP_TYPE, data: normalTab}]],
        null, aNormalWindow, aPrivateWindow);
      is(effect, "none", "Should not be able to drag a normal tab to a private window");

      effect = ChromeUtils.synthesizeDrop(privateTab, normalTab,
        [[{type: TAB_DROP_TYPE, data: privateTab}]],
        null, aPrivateWindow, aNormalWindow);
      is(effect, "none", "Should not be able to drag a private tab to a normal window");

      aNormalWindow.gBrowser.swapBrowsersAndCloseOther(normalTab, privateTab);
      is(aNormalWindow.gBrowser.tabs.length, 2, "Prevent moving a normal tab to a private tabbrowser");
      is(aPrivateWindow.gBrowser.tabs.length, 2, "Prevent accepting a normal tab in a private tabbrowser");

      aPrivateWindow.gBrowser.swapBrowsersAndCloseOther(privateTab, normalTab);
      is(aPrivateWindow.gBrowser.tabs.length, 2, "Prevent moving a private tab to a normal tabbrowser");
      is(aNormalWindow.gBrowser.tabs.length, 2, "Prevent accepting a private tab in a normal tabbrowser");

      aNormalWindow.close();
      aPrivateWindow.close();
      finish();
    });
  });
}

