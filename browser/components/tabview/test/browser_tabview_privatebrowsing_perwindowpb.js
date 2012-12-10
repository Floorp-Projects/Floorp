/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let normalURLs = [];
let pbTabURL = "about:privatebrowsing";
let groupTitles = [];
let normalIteration = 0;

function test() {
  waitForExplicitFinish();

  testOnWindow(false, function(win) {
    showTabView(function() {
      onTabViewLoadedAndShown(win);
    }, win);
  });
}

function testOnWindow(aIsPrivate, aCallback) {
  let win = OpenBrowserWindow({private: aIsPrivate});
  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);
    executeSoon(function() { aCallback(win); });
  }, false);
}

function onTabViewLoadedAndShown(aWindow) {
  ok(aWindow.TabView.isVisible(), "Tab View is visible");

  // Establish initial state
  let contentWindow = aWindow.TabView.getContentWindow();
  verifyCleanState(aWindow, "start", contentWindow);

  // create a group
  let box = new contentWindow.Rect(20, 20, 180, 180);
  let groupItem = new contentWindow.GroupItem([], {bounds: box, title: "test1"});
  let id = groupItem.id;
  is(contentWindow.GroupItems.groupItems.length, 2, "we now have two groups");
  registerCleanupFunction(function() {
    contentWindow.GroupItems.groupItem(id).close();
  });

  // make it the active group so new tabs will be added to it
  contentWindow.UI.setActive(groupItem);

  // collect the group titles
  let count = contentWindow.GroupItems.groupItems.length;
  for (let a = 0; a < count; a++) {
    let gi = contentWindow.GroupItems.groupItems[a];
    groupTitles[a] = gi.getTitle();
  }

  contentWindow.gPrefBranch.setBoolPref("animate_zoom", false);

  // Create a second tab
  aWindow.gBrowser.addTab("about:mozilla");
  is(aWindow.gBrowser.tabs.length, 2, "we now have 2 tabs");

  registerCleanupFunction(function() {
    aWindow.gBrowser.removeTab(aWindow.gBrowser.tabs[1]);
    contentWindow.gPrefBranch.clearUserPref("animate_zoom");
  });

  afterAllTabsLoaded(function() {
    // Get normal tab urls
    for (let a = 0; a < aWindow.gBrowser.tabs.length; a++)
      normalURLs.push(aWindow.gBrowser.tabs[a].linkedBrowser.currentURI.spec);

    // verify that we're all set up for our test
    verifyNormal(aWindow);

    // Open private window and make sure Tab View becomes hidden
    testOnWindow(true, function(privateWin) {
      whenTabViewIsHidden(function() {
        ok(!privateWin.TabView.isVisible(), "Tab View is no longer visible");
        verifyPB(privateWin);
        // Close private window
        privateWin.close();
        // Get back to normal window, make sure Tab View is shown again
        whenTabViewIsShown(function() {
          ok(aWindow.TabView.isVisible(), "Tab View is visible again");
          verifyNormal(aWindow);

          hideTabView(function() {
            onTabViewHidden(aWindow, contentWindow);
          }, aWindow);
        }, aWindow);
      }, privateWin);
    });
  }, aWindow);
}

function onTabViewHidden(aWindow, aContentWindow) {
  ok(!aWindow.TabView.isVisible(), "Tab View is not visible");

  // Open private window and make sure Tab View is hidden
  testOnWindow(true, function(privateWin) {
    ok(!privateWin.TabView.isVisible(), "Tab View is still not visible");
    verifyPB(privateWin);
    // Close private window
    privateWin.close();

    //  Get back to normal window
    verifyNormal(aWindow);

    // end game
    ok(!aWindow.TabView.isVisible(), "we finish with Tab View not visible");

    // verify after all cleanups
    registerCleanupFunction(function() {
      verifyCleanState(aWindow, "finish", aContentWindow);
      aWindow.close();
    });

    finish();
  });
}

function verifyCleanState(aWindow, aMode, aContentWindow) {
  let prefix = "we " + aMode + " with ";
  is(aWindow.gBrowser.tabs.length, 1, prefix + "one tab");
  is(aContentWindow.GroupItems.groupItems.length, 1, prefix + "1 group");
  ok(aWindow.gBrowser.tabs[0]._tabViewTabItem.parent == aContentWindow.GroupItems.groupItems[0],
      "the tab is in the group");
  ok(!PrivateBrowsingUtils.isWindowPrivate(aWindow), prefix + "private browsing off");
}

function verifyPB(aWindow) {
  showTabView(function() {
    let cw = aWindow.TabView.getContentWindow();
    ok(PrivateBrowsingUtils.isWindowPrivate(aWindow), "window is private");
    is(aWindow.gBrowser.tabs.length, 1, "we have 1 tab in private window");
    is(cw.GroupItems.groupItems.length, 1, "we have 1 group in private window");
    ok(aWindow.gBrowser.tabs[0]._tabViewTabItem.parent == cw.GroupItems.groupItems[0],
        "the tab is in the group");
    let browser = aWindow.gBrowser.tabs[0].linkedBrowser;
    browser.addEventListener("load", function onLoad() {
      browser.removeEventListener("load", onLoad, true);
      is(browser.currentURI.spec, pbTabURL, "correct URL for private browsing");
    }, true);
  }, aWindow);
}

function verifyNormal(aWindow) {
  let contentWindow = aWindow.TabView.getContentWindow();
  let prefix = "verify normal " + normalIteration + ": ";
  normalIteration++;

  ok(!PrivateBrowsingUtils.isWindowPrivate(aWindow), prefix + "private browsing off");

  let tabCount = aWindow.gBrowser.tabs.length;
  let groupCount = contentWindow.GroupItems.groupItems.length;
  is(tabCount, 2, prefix + "we have 2 tabs");
  is(groupCount, 2, prefix + "we have 2 groups");
  ok(tabCount == groupCount, prefix + "same number of tabs as groups");
  for (let a = 0; a < tabCount; a++) {
    let tab = aWindow.gBrowser.tabs[a];
    is(tab.linkedBrowser.currentURI.spec, normalURLs[a],
        prefix + "correct URL");

    let groupItem = contentWindow.GroupItems.groupItems[a];
    is(groupItem.getTitle(), groupTitles[a], prefix + "correct group title");

    ok(tab._tabViewTabItem.parent == groupItem,
        prefix + "tab " + a + " is in group " + a);
  }
}
