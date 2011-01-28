/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is bug 627239 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

function test() {
  waitForExplicitFinish();
  
  // We want to actually see what information is cached via sessionstore,
  // so we can't let tabs themselves get restored immediately.
  Services.prefs.setIntPref("browser.sessionstore.max_concurrent_tabs", 0);
  
  newWindowWithTabView(onTabViewWindowLoaded);
}

registerCleanupFunction(function() {
  Services.prefs.clearUserPref("browser.sessionstore.max_concurrent_tabs");
})

function onTabViewWindowLoaded(win) {
  ok(win.TabView.isVisible(), "Tab View is visible");

  let contentWindow = win.document.getElementById("tab-view").contentWindow;
  let [originalTab] = win.gBrowser.visibleTabs;
  let originalGroup = contentWindow.GroupItems.getActiveGroupItem();
  let originalURL   = contentWindow.TabUtils.URLOf(originalTab);

  // Create secure and insecure tabs, but don't change the active tab.
  let insecure = win.gBrowser.loadOneTab("http://example.com", {inBackground: true});
  let secure   = win.gBrowser.loadOneTab("https://example.com", {inBackground: true});
  
  // We're going to do some state validation, close the window, and reopen it.
  let restoredWindow;
  
  function checkState() {
    is(win.gBrowser.tabs.length, 3, "There are three tabs");
    is(win.gBrowser.visibleTabs.length, 3, "All are visible");
    is(contentWindow.UI.getActiveTab(), originalTab._tabViewTabItem,
      "The original tab is active (Panorama)");
    is(win.gBrowser.selectedTab, originalTab,
      "The original tab is active (tabbrowser)");

    // force a saving of image data
    contentWindow.TabItems.saveAll(true);
    ok(contentWindow.Storage.getTabData(insecure).imageData,
      "Insecure tab has imageData");
    ok(!contentWindow.Storage.getTabData(secure).imageData,
      "Secure tab doesn't have imageData");

    Services.obs.addObserver(
      xulWindowDestroy, "xul-window-destroyed", false);
    win.close();
  }

  function xulWindowDestroy() {
    Services.obs.removeObserver(
       xulWindowDestroy, "xul-window-destroyed", false);
    executeSoon(function() {
      restoredWindow = undoCloseWindow(0);
      restoredWindow.addEventListener("load", function() {
        restoredWindow.removeEventListener("load", arguments.callee, false);
        showTabView(checkRestored, restoredWindow);
      }, false);
    });
  }
  
  function checkRestored() {
    let contentWindow = restoredWindow.document.getElementById("tab-view").
                        contentWindow;
    let TabUtils = contentWindow.TabUtils;
    let tabs = restoredWindow.gBrowser.tabs;
    is(tabs.length, 3, "There are three tabs");
    is(restoredWindow.gBrowser.visibleTabs.length, 3, "All are visible");
    is(TabUtils.URLOf(restoredWindow.gBrowser.selectedTab), originalURL,
      "The original tab is active");

    afterAllTabItemsUpdated(function() {
      ok(tabs[1]._tabViewTabItem.isShowingCachedData(),
         "Insecure tab is showing cached data");
      ok(!tabs[2]._tabViewTabItem.isShowingCachedData(),
         "Secure tab is not showing cached data");
  
      restoredWindow.close();
      finish();
    }, restoredWindow);
  }
    
  afterAllTabsLoaded(function() {
    afterAllTabItemsUpdated(function () {
      hideTabView(checkState, win);
    }, win);
  }, win);
}
