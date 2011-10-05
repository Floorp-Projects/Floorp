/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

let pb = Cc['@mozilla.org/privatebrowsing;1'].
         getService(Ci.nsIPrivateBrowsingService);

function test() {
  let tests = [];

  let getContentWindow = function () {
    return TabView.getContentWindow();
  }

  let assertOneSingleGroupItem = function () {
    is(getContentWindow().GroupItems.groupItems.length, 1, 'There is one single groupItem');
  }

  let assertNumberOfVisibleTabs = function (numTabs) {
    is(gBrowser.visibleTabs.length, numTabs, 'There should be ' + numTabs + ' visible tabs');
  }

  let next = function () {
    while (gBrowser.tabs.length-1)
      gBrowser.removeTab(gBrowser.tabs[1]);

    hideTabView(function () {
      let callback = tests.shift();

      if (!callback)
        callback = finish;

      assertOneSingleGroupItem();
      callback();
    });
  }

  // ----------
  // [624265] testing undo close tab
  let testUndoCloseTabs = function () {
    gBrowser.loadOneTab('http://mochi.test:8888/', {inBackground: true});
    gBrowser.loadOneTab('http://mochi.test:8888/', {inBackground: true});

    afterAllTabsLoaded(function () {
      assertNumberOfVisibleTabs(3);

      gBrowser.removeTab(gBrowser.tabs[1]);
      gBrowser.selectedTab = gBrowser.tabs[1];

      restoreTab(function () {
        assertNumberOfVisibleTabs(3);
        assertOneSingleGroupItem();
        next();
      });
    });
  }

  // ----------
  // [623792] duplicating tab via middle click on reload button
  let testDuplicateTab = function () {
    gBrowser.loadOneTab('http://mochi.test:8888/', {inBackground: true});

    afterAllTabsLoaded(function () {
      // Valid choices for 'where' are window|tabshifted|tab
      duplicateTabIn(gBrowser.selectedTab, 'tab');

      afterAllTabsLoaded(function () {
        assertNumberOfVisibleTabs(3);
        assertOneSingleGroupItem();
        next();
      });
    });
  }

  // ----------
  // [623792] duplicating tabs via middle click on forward/back buttons
  let testBackForwardDuplicateTab = function () {
    let tab = gBrowser.loadOneTab('http://mochi.test:8888/#1', {inBackground: true});
    gBrowser.selectedTab = tab;

    afterAllTabsLoaded(function () {
      tab.linkedBrowser.loadURI('http://mochi.test:8888/#2');

      afterAllTabsLoaded(function () {
        ok(gBrowser.canGoBack, 'browser can go back in history');
        BrowserBack({button: 1});

        afterAllTabsLoaded(function () {
          assertNumberOfVisibleTabs(3);

          ok(gBrowser.canGoForward, 'browser can go forward in history');
          BrowserForward({button: 1});

          afterAllTabsLoaded(function () {
            assertNumberOfVisibleTabs(4);
            assertOneSingleGroupItem();
            next();
          });
        });
      });
    });
  }

  // ----------
  // [624102] check state after return from private browsing
  let testPrivateBrowsing = function () {
    gBrowser.loadOneTab('http://mochi.test:8888/#1', {inBackground: true});
    gBrowser.loadOneTab('http://mochi.test:8888/#2', {inBackground: true});

    let cw = getContentWindow();
    let box = new cw.Rect(20, 20, 250, 200);
    let groupItem = new cw.GroupItem([], {bounds: box, immediately: true});
    cw.UI.setActive(groupItem);

    gBrowser.selectedTab = gBrowser.loadOneTab('http://mochi.test:8888/#3', {inBackground: true});
    gBrowser.loadOneTab('http://mochi.test:8888/#4', {inBackground: true});

    afterAllTabsLoaded(function () {
      assertNumberOfVisibleTabs(2);

      enterAndLeavePrivateBrowsing(function () {
        assertNumberOfVisibleTabs(2);
        gBrowser.selectedTab = gBrowser.tabs[0];
        closeGroupItem(cw.GroupItems.groupItems[1], next);
      });
    });
  }

  waitForExplicitFinish();

  // tests for #624265
  tests.push(testUndoCloseTabs);

  // tests for #623792
  tests.push(testDuplicateTab);
  tests.push(testBackForwardDuplicateTab);

  // tests for #624102
  tests.push(testPrivateBrowsing);

  loadTabView(next);
}

// ----------
function loadTabView(callback) {
  showTabView(function () {
    hideTabView(callback);
  });
}

// ----------
function enterAndLeavePrivateBrowsing(callback) {
  togglePrivateBrowsing(function () {
    togglePrivateBrowsing(callback);
  });
}
