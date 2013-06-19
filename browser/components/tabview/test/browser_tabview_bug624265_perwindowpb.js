/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  let tests = [];

  let getContentWindow = function (aWindow) {
    return aWindow.TabView.getContentWindow();
  }

  let assertOneSingleGroupItem = function (aWindow) {
    is(getContentWindow(aWindow).GroupItems.groupItems.length, 1, 'There is one single groupItem');
  }

  let assertNumberOfVisibleTabs = function (aWindow, numTabs) {
    is(aWindow.gBrowser.visibleTabs.length, numTabs, 'There should be ' + numTabs + ' visible tabs');
  }

  let next = function (aWindow) {
    while (aWindow.gBrowser.tabs.length-1)
      aWindow.gBrowser.removeTab(aWindow.gBrowser.tabs[1]);

    hideTabView(function() {
      let callback = tests.shift();

      if (!callback) {
        executeSoon(function() {
          assertOneSingleGroupItem(aWindow);
          aWindow.close();
          finish();
        });
      } else {
        assertOneSingleGroupItem(aWindow);
        callback(aWindow);
      }
    }, aWindow);
  }

  // [624265] testing undo close tab
  let testUndoCloseTabs = function (aWindow) {
    aWindow.gBrowser.loadOneTab('http://mochi.test:8888/', {inBackground: true});
    aWindow.gBrowser.loadOneTab('http://mochi.test:8888/', {inBackground: true});

    afterAllTabsLoaded(function () {
      assertNumberOfVisibleTabs(aWindow, 3);

      aWindow.gBrowser.removeTab(aWindow.gBrowser.tabs[1]);
      aWindow.gBrowser.selectedTab = aWindow.gBrowser.tabs[1];

      restoreTab(function () {
        assertNumberOfVisibleTabs(aWindow, 3);
        assertOneSingleGroupItem(aWindow);
        next(aWindow);
      }, 0, aWindow);
    }, aWindow);
  }

  // [623792] duplicating tab via middle click on reload button
  let testDuplicateTab = function (aWindow) {
    aWindow.gBrowser.loadOneTab('http://mochi.test:8888/', {inBackground: true});

    afterAllTabsLoaded(function () {
      // Valid choices for 'where' are window|tabshifted|tab
      aWindow.duplicateTabIn(aWindow.gBrowser.selectedTab, 'tab');

      afterAllTabsLoaded(function () {
        assertNumberOfVisibleTabs(aWindow, 3);
        assertOneSingleGroupItem(aWindow);
        next(aWindow);
      }, aWindow);
    }, aWindow);
  }

  // [623792] duplicating tabs via middle click on forward/back buttons
  let testBackForwardDuplicateTab = function (aWindow) {
    let tab = aWindow.gBrowser.loadOneTab('http://mochi.test:8888/#1', {inBackground: true});
    aWindow.gBrowser.selectedTab = tab;

    afterAllTabsLoaded(function () {
      tab.linkedBrowser.loadURI('http://mochi.test:8888/#2');

      afterAllTabsLoaded(function () {
        ok(aWindow.gBrowser.canGoBack, 'browser can go back in history');
        aWindow.BrowserBack({button: 1});

        afterAllTabsLoaded(function () {
          assertNumberOfVisibleTabs(aWindow, 3);

          ok(aWindow.gBrowser.canGoForward, 'browser can go forward in history');
          aWindow.BrowserForward({button: 1});

          afterAllTabsLoaded(function () {
            assertNumberOfVisibleTabs(aWindow, 4);
            assertOneSingleGroupItem(aWindow);
            next(aWindow);
          }, aWindow);
        }, aWindow);
      }, aWindow);
    }, aWindow);
  }

  function testOnWindow(aCallback) {
    let win = OpenBrowserWindow({private: false});
    win.addEventListener("load", function onLoad() {
      win.removeEventListener("load", onLoad, false);
      executeSoon(function() { aCallback(win) });
    }, false);
  }

  waitForExplicitFinish();

  // Tests for #624265
  tests.push(testUndoCloseTabs);

  // Tests for #623792
  tests.push(testDuplicateTab);
  tests.push(testBackForwardDuplicateTab);

  testOnWindow(function(aWindow) {
    loadTabView(function() {
      next(aWindow);
    }, aWindow);
  });
}

function loadTabView(callback, aWindow) {
  showTabView(function () {
    hideTabView(callback, aWindow);
  }, aWindow);
}
