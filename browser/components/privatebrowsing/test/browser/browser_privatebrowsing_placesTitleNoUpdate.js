/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test to make sure that the visited page titles do not get updated inside the
// private browsing mode.

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/PlacesUtils.jsm");

function test() {
  waitForExplicitFinish();
  const TEST_URL = "http://mochi.test:8888/browser/browser/components/privatebrowsing/test/browser/browser_privatebrowsing_placesTitleNoUpdate.html"
  const TEST_URI = Services.io.newURI(TEST_URL, null, null);
  const TITLE_1 = "Title 1";
  const TITLE_2 = "Title 2";

  let selectedWin = null;
  let windowsToClose = [];
  let tabToClose = null;
  let testNumber = 0;
  let historyObserver;


  registerCleanupFunction(function() {
    PlacesUtils.history.removeObserver(historyObserver, false);
    windowsToClose.forEach(function(aWin) {
      aWin.close();
    });
    gBrowser.removeTab(tabToClose);
  });


  waitForClearHistory(function () {
    historyObserver = {
      onTitleChanged: function(aURI, aPageTitle) {
        switch (++testNumber) {
          case 1:
            afterFirstVisit();
          break;
          case 2:
            afterUpdateVisit();
          break;
        }
      },
      onBeginUpdateBatch: function () {},
      onEndUpdateBatch: function () {},
      onVisit: function () {},
      onDeleteURI: function () {},
      onClearHistory: function () {},
      onPageChanged: function () {},
      onDeleteVisits: function() {},
      QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
    };
    PlacesUtils.history.addObserver(historyObserver, false);

    tabToClose = gBrowser.addTab();
    gBrowser.selectedTab = tabToClose;
    whenPageLoad(window, function() {});
  });

  function afterFirstVisit() {
    is(PlacesUtils.history.getPageTitle(TEST_URI), TITLE_1, "The title matches the orignal title after first visit");

    let place = {
      uri: TEST_URI,
      title: TITLE_2,
      visits: [{
        visitDate: Date.now() * 1000,
        transitionType: Ci.nsINavHistoryService.TRANSITION_LINK
      }]
    };
    PlacesUtils.asyncHistory.updatePlaces(place, {
      handleError: function () do_throw("Unexpected error in adding visit."),
      handleResult: function () { },
      handleCompletion: function () {}
    });
  }

  function afterUpdateVisit() {
    is(PlacesUtils.history.getPageTitle(TEST_URI), TITLE_2, "The title matches the updated title after updating visit");

    testOnWindow(true, function(aWin) {
      whenPageLoad(aWin, function() {
        executeSoon(afterFirstVisitInPrivateWindow);
      });
    });
  }

  function afterFirstVisitInPrivateWindow() {
     is(PlacesUtils.history.getPageTitle(TEST_URI), TITLE_2, "The title remains the same after visiting in private window");
     waitForClearHistory(finish);
  }

  function whenPageLoad(aWin, aCallback) {
    aWin.gBrowser.selectedBrowser.addEventListener("load", function onLoad() {
      aWin.gBrowser.selectedBrowser.removeEventListener("load", onLoad, true);
      aCallback();
    }, true);
    aWin.gBrowser.selectedBrowser.loadURI(TEST_URL);
  }

  function testOnWindow(aPrivate, aCallback) {
    whenNewWindowLoaded({ private: aPrivate }, function(aWin) {
      selectedWin = aWin;
      windowsToClose.push(aWin);
      executeSoon(function() { aCallback(aWin) });
    });
  }

  function waitForClearHistory(aCallback) {
    let observer = {
      observe: function(aSubject, aTopic, aData) {
        Services.obs.removeObserver(this, PlacesUtils.TOPIC_EXPIRATION_FINISHED);
        aCallback();
      }
    };
    Services.obs.addObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED, false);

    PlacesUtils.bhistory.removeAllPages();
  }
}

