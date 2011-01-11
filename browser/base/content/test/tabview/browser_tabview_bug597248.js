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
 * The Original Code is tabview bug 597248 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Raymond Lee <raymond@appcoast.com>
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

let newWin;
let restoredWin;
let newTabOne;
let newTabTwo;
let restoredNewTabOneLoaded = false;
let restoredNewTabTwoLoaded = false;
let frameInitialized = false;

function test() {
  waitForExplicitFinish();

  // open a new window 
  newWin = openDialog(getBrowserURL(), "_blank", "chrome,all,dialog=no");
  newWin.addEventListener("load", function(event) {
    newWin.removeEventListener("load", arguments.callee, false);
    setupOne();
  }, false);
}

function setupOne() {
  let loadedCount = 0;
  let allLoaded = function() {
    if (++loadedCount == 2) {
      newWin.addEventListener("tabviewshown", setupTwo, false);
      newWin.TabView.toggle();
    }
  }
  
  newTabOne = newWin.gBrowser.tabs[0];
  newTabTwo = newWin.gBrowser.addTab();
  load(newTabOne, "http://mochi.test:8888/browser/browser/base/content/test/tabview/search1.html", allLoaded);
  load(newTabTwo, "http://mochi.test:8888/browser/browser/base/content/test/tabview/dummy_page.html", allLoaded);
}

function setupTwo() {
  newWin.removeEventListener("tabviewshown", setupTwo, false);

  let contentWindow = newWin.document.getElementById("tab-view").contentWindow;

  let tabItems = contentWindow.TabItems.getItems();
  is(tabItems.length, 2, "There should be 2 tab items before closing");

  // force all canvas to update
  tabItems.forEach(function(tabItem) {
    contentWindow.TabItems._update(tabItem.tab);
  });

  // after the window is closed, restore it.
  let xulWindowDestory = function() {
    Services.obs.removeObserver(
       xulWindowDestory, "xul-window-destroyed", false);

    newWin = null;
    // "xul-window-destroyed" is just fired just before a XUL window is
    // destroyed so restore window and test it after a delay
    executeSoon(function() {
      restoredWin = undoCloseWindow();
      restoredWin.addEventListener("load", function(event) {
        restoredWin.removeEventListener("load", arguments.callee, false);

        // execute code when the frame is initialized.
        let onTabViewFrameInitialized = function() {
          restoredWin.removeEventListener(
            "tabviewframeinitialized", onTabViewFrameInitialized, false);

          /*
          // bug 615954 happens too often so we disable this until we have a fix
          let restoredContentWindow = 
            restoredWin.document.getElementById("tab-view").contentWindow;
          // prevent TabItems._update being called before checking cached images
          restoredContentWindow.TabItems._pauseUpdateForTest = true;
          */
          restoredWin.close();
          finish();
        }
        restoredWin.addEventListener(
          "tabviewframeinitialized", onTabViewFrameInitialized, false);

        is(restoredWin.gBrowser.tabs.length, 2, "The total number of tabs is 2");

        /*
        // bug 615954 happens too often so we disable this until we have a fix
        restoredWin.addEventListener("tabviewshown", onTabViewShown, false);

        // setup tab variables and listen to the load progress.
        newTabOne = restoredWin.gBrowser.tabs[0];
        newTabTwo = restoredWin.gBrowser.tabs[1];
        restoredWin.gBrowser.addTabsProgressListener(gTabsProgressListener);
        */
      }, false);
    });
  };

  // check the storage for stored image data
  let checkDataAndCloseWindow = function() {
    tabItems.forEach(function(tabItem) {
      let tabData = contentWindow.Storage.getTabData(tabItem.tab);
      ok(tabData && tabData.imageData,
        "TabItem has stored image data before closing");
    });

    Services.obs.addObserver(
      xulWindowDestory, "xul-window-destroyed", false);
    newWin.close();
  }

  // stimulate a quit application requested so the image data gets stored
  let quitRequestObserver = function(aSubject, aTopic, aData) {
    ok(aTopic == "quit-application-requested" &&
        aSubject instanceof Ci.nsISupportsPRBool,
        "Received a quit request and going to deny it");
    Services.obs.removeObserver(
      quitRequestObserver, "quit-application-requested", false);
    // cancel the shut down
    aSubject.data = true;
    // save all images is execuated when "quit-application-requested" topic is 
    // announced so executeSoon is used to avoid racing condition.
    executeSoon(checkDataAndCloseWindow);
  }
  Services.obs.addObserver(
    quitRequestObserver, "quit-application-requested", false);
  ok(!Application.quit(), "Tried to quit and canceled it");
}

let gTabsProgressListener = {
  onStateChange: function(browser, webProgress, request, stateFlags, status) {
    // ensure about:blank doesn't trigger the code
    if ((stateFlags & Ci.nsIWebProgressListener.STATE_STOP) &&
        (stateFlags & Ci.nsIWebProgressListener.STATE_IS_WINDOW) &&
         browser.currentURI.spec != "about:blank") {
      if (newTabOne.linkedBrowser == browser)
        restoredNewTabOneLoaded = true;
      else if (newTabTwo.linkedBrowser == browser)
        restoredNewTabTwoLoaded = true;

      // since we are not sure whether the frame is initialized first or two tabs
      // compete loading first so we need this.
      if (restoredNewTabOneLoaded && restoredNewTabTwoLoaded) {
        restoredWin.gBrowser.removeTabsProgressListener(gTabsProgressListener);

        if (frameInitialized) {
          // since a tabs progress listener is used in the code to set 
          // tabItem.shouldHideCachedData, executeSoon is used to avoid a racing
          // condition.
          executeSoon(updateAndCheck); 
        }
      }
    }
  }
};

function onTabViewShown() {
  restoredWin.removeEventListener("tabviewshown", onTabViewShown, false);

  let contentWindow = 
    restoredWin.document.getElementById("tab-view").contentWindow;

  let nextStep = function() {
    // since we are not sure whether the frame is initialized first or two tabs
    // compete loading first so we need this.
    if (restoredNewTabOneLoaded && restoredNewTabTwoLoaded) {
      // executeSoon is used to ensure tabItem.shouldHideCachedData is set
      // because tabs progress listener might run at the same time as this test 
      // code.
      executeSoon(updateAndCheck);
    } else {
      frameInitialized = true;
    }
  }

  let tabItems = contentWindow.TabItems.getItems();
  let count = tabItems.length;
  tabItems.forEach(function(tabItem) {
    // tabitem might not be connected so use subscriber for those which are not
    // connected.
    if (tabItem._reconnected) {
      ok(tabItem.isShowingCachedData(), 
         "Tab item is showing cached data and is already connected. " +
         tabItem.tab.linkedBrowser.currentURI.spec);
      if (--count == 0)
        nextStep();
    } else {
      tabItem.addSubscriber(tabItem, "reconnected", function() {
        tabItem.removeSubscriber(tabItem, "reconnected");
        ok(tabItem.isShowingCachedData(), 
           "Tab item is showing cached data and is just connected. "  +
           tabItem.tab.linkedBrowser.currentURI.spec);
        if (--count == 0)
          nextStep();
      });
    }
  });
}

function updateAndCheck() {
  // force all canvas to update
  let contentWindow = 
    restoredWin.document.getElementById("tab-view").contentWindow;

  contentWindow.TabItems._pauseUpdateForTest = false;

  let tabItems = contentWindow.TabItems.getItems();
  tabItems.forEach(function(tabItem) {
    contentWindow.TabItems._update(tabItem.tab);
    ok(!tabItem.isShowingCachedData(), 
      "Tab item is not showing cached data anymore. " +
      tabItem.tab.linkedBrowser.currentURI.spec);
  });

  // clean up and finish
  restoredWin.close();
  finish();
}

function load(tab, url, callback) {
  tab.linkedBrowser.addEventListener("load", function (event) {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    callback();
  }, true);
  tab.linkedBrowser.loadURI(url);
}

