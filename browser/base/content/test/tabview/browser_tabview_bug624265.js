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

  let restoreTab = function (callback) {
    let tab = undoCloseTab(0);
    
    tab._tabViewTabItem.addSubscriber(tab, 'reconnected', function () {
      tab._tabViewTabItem.removeSubscriber(tab, 'reconnected');
      afterAllTabsLoaded(callback);
    });
  }

  let next = function () {
    while (gBrowser.tabs.length-1)
      gBrowser.removeTab(gBrowser.tabs[1]);

    hideTabView(function () {
      let callback = tests.shift();

      if (!callback)
        callback = finish;

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

    let continueTest = function () {
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
    }

    // The executeSoon() call is really needed here because there's probably
    // some callback waiting to be fired after gBrowser.loadOneTab(). After
    // that the browser is in a state where loadURI() will create a new entry
    // in the session history (that is vital for back/forward functionality).
    afterAllTabsLoaded(function () SimpleTest.executeSoon(continueTest));
  }

  // ----------
  // [624102] check state after return from private browsing
  let testPrivateBrowsing = function () {
    gBrowser.loadOneTab('http://mochi.test:8888/#1', {inBackground: true});
    gBrowser.loadOneTab('http://mochi.test:8888/#2', {inBackground: true});

    let cw = getContentWindow();
    let box = new cw.Rect(20, 20, 250, 200);
    let groupItem = new cw.GroupItem([], {bounds: box, immediately: true});
    cw.GroupItems.setActiveGroupItem(groupItem);

    gBrowser.selectedTab = gBrowser.loadOneTab('http://mochi.test:8888/#3', {inBackground: true});
    gBrowser.loadOneTab('http://mochi.test:8888/#4', {inBackground: true});

    afterAllTabsLoaded(function () {
      assertNumberOfVisibleTabs(2);

      enterAndLeavePrivateBrowsing(function () {
        assertNumberOfVisibleTabs(2);
        next();
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
  window.addEventListener('tabviewshown', function () {
    window.removeEventListener('tabviewshown', arguments.callee, false);

    hideTabView(function () {
      window.removeEventListener('tabviewhidden', arguments.callee, false);
      callback();
    });
  }, false);

  TabView.show();
}

// ----------
function hideTabView(callback) {
  if (!TabView.isVisible())
    return callback();

  window.addEventListener('tabviewhidden', function () {
    window.removeEventListener('tabviewhidden', arguments.callee, false);
    callback();
  }, false);

  TabView.hide();
}

// ----------
function enterAndLeavePrivateBrowsing(callback) {
  function pbObserver(aSubject, aTopic, aData) {
    if (aTopic != "private-browsing-transition-complete")
      return;

    if (pb.privateBrowsingEnabled)
      pb.privateBrowsingEnabled = false;
    else {
      Services.obs.removeObserver(pbObserver, "private-browsing-transition-complete");
      afterAllTabsLoaded(callback);
    }
  }

  Services.obs.addObserver(pbObserver, "private-browsing-transition-complete", false);
  pb.privateBrowsingEnabled = true;
}
