/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let newTabOne;
let newTabTwo;
let restoredNewTabOneLoaded = false;
let restoredNewTabTwoLoaded = false;
let frameInitialized = false;

function test() {
  waitForExplicitFinish();
  newWindowWithTabView(setupOne);
}

function setupOne(win) {
  win.TabView.firstUseExperienced = true;

  win.gBrowser.addTab("http://mochi.test:8888/browser/browser/base/content/test/tabview/search1.html");
  win.gBrowser.addTab("http://mochi.test:8888/browser/browser/base/content/test/tabview/dummy_page.html");

  afterAllTabsLoaded(function () setupTwo(win), win);
}

function setupTwo(win) {
  let contentWindow = win.TabView.getContentWindow();

  let tabItems = contentWindow.TabItems.getItems();
  is(tabItems.length, 3, "There should be 3 tab items before closing");

  let numTabsToSave = tabItems.length;

  // force all canvases to update, and hook in imageData save detection
  tabItems.forEach(function(tabItem) {
    contentWindow.TabItems._update(tabItem.tab);
    tabItem.addSubscriber(tabItem, "savedCachedImageData", function(item) {
      item.removeSubscriber(item, "savedCachedImageData");
      --numTabsToSave;
    });
  });

  // after the window is closed, restore it.
  let xulWindowDestory = function() {
    Services.obs.removeObserver(
       xulWindowDestory, "xul-window-destroyed", false);

    // "xul-window-destroyed" is just fired just before a XUL window is
    // destroyed so restore window and test it after a delay
    executeSoon(function() {
      let restoredWin = undoCloseWindow();
      restoredWin.addEventListener("load", function onLoad(event) {
        restoredWin.removeEventListener("load", onLoad, false);

        // ensure that closed tabs have been saved
        is(numTabsToSave, 0, "All tabs were saved when window was closed.");

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

        is(restoredWin.gBrowser.tabs.length, 3, "The total number of tabs is 3");

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

  Services.obs.addObserver(
    xulWindowDestory, "xul-window-destroyed", false);

  win.close();
}

/*let gTabsProgressListener = {
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
}*/
