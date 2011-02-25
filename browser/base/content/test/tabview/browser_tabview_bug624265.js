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
 * The Original Code is tabview bug 624265 test.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Tim Taubert <tim.taubert@gmx.de>
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
      duplicateTabIn(gBrowser.selectedTab, 'current');

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
