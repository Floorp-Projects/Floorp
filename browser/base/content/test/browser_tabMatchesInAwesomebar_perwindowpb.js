/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const TEST_URL_BASES = [
  "http://example.org/browser/browser/base/content/test/dummy_page.html#tabmatch",
  "http://example.org/browser/browser/base/content/test/moz.png#tabmatch"
];

var gController = Cc["@mozilla.org/autocomplete/controller;1"].
                  getService(Ci.nsIAutoCompleteController);

var gTabWaitCount = 0;
var gTabCounter = 0;

var gTestSteps = [
  function() {
    info("Running step 1");
    for (let i = 0; i < 10; i++) {
      let tab = gBrowser.addTab();
      loadTab(tab, TEST_URL_BASES[0] + (++gTabCounter));
    }
  },
  function() {
    info("Running step 2");
    gBrowser.selectTabAtIndex(1);
    gBrowser.removeCurrentTab();
    gBrowser.selectTabAtIndex(1);
    gBrowser.removeCurrentTab();
    for (let i = 1; i < gBrowser.tabs.length; i++)
      loadTab(gBrowser.tabs[i], TEST_URL_BASES[1] + (++gTabCounter));
  },
  function() {
    info("Running step 3");
    for (let i = 1; i < gBrowser.tabs.length; i++)
      loadTab(gBrowser.tabs[i], TEST_URL_BASES[0] + gTabCounter);
  },
  function() {
    info("Running step 4 - ensure we don't register subframes as open pages");
    let tab = gBrowser.addTab();
    tab.linkedBrowser.addEventListener("load", function () {
      tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
      // Start the sub-document load.
      executeSoon(function () {
        tab.linkedBrowser.addEventListener("load", function (e) {
          tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
            ensure_opentabs_match_db(nextStep);
        }, true);
        tab.linkedBrowser.contentDocument.querySelector("iframe").src = "http://test2.example.org/";
      });
    }, true);
    tab.linkedBrowser.loadURI('data:text/html,<body><iframe src=""></iframe></body>');
  },
  function() {
    info("Running step 5 - remove tab immediately");
    let tab = gBrowser.addTab("about:logo");
    gBrowser.removeTab(tab);
    ensure_opentabs_match_db(nextStep);
  },
  function() {
    info("Running step 6 - check swapBrowsersAndCloseOther preserves registered switch-to-tab result");
    let tabToKeep = gBrowser.addTab();
    let tab = gBrowser.addTab();
    tab.linkedBrowser.addEventListener("load", function () {
      tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
      gBrowser.swapBrowsersAndCloseOther(tabToKeep, tab);
      ensure_opentabs_match_db(function () {
        gBrowser.removeTab(tabToKeep);
        ensure_opentabs_match_db(nextStep);
      });
    }, true);
    tab.linkedBrowser.loadURI("about:mozilla");
  },
  function() {
    info("Running step 7 - close all tabs");

    Services.prefs.clearUserPref("browser.sessionstore.restore_on_demand");

    gBrowser.addTab("about:blank", {skipAnimation: true});
    while (gBrowser.tabs.length > 1) {
      info("Removing tab: " + gBrowser.tabs[0].linkedBrowser.currentURI.spec);
      gBrowser.selectTabAtIndex(0);
      gBrowser.removeCurrentTab();
    }
    ensure_opentabs_match_db(nextStep);
  }
];



function test() {
  waitForExplicitFinish();
  nextStep();
}

function loadTab(tab, url) {
  // Because adding visits is async, we will not be notified immediately.
  let visited = false;
  let loaded = false;

  function maybeCheckResults() {
    if (visited && loaded && --gTabWaitCount == 0) {
      ensure_opentabs_match_db(nextStep);
    }
  }

  tab.linkedBrowser.addEventListener("load", function () {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    loaded = true;
    maybeCheckResults();
  }, true);

  if (!visited) {
    Services.obs.addObserver(
      function (aSubject, aTopic, aData) {
        if (url != aSubject.QueryInterface(Ci.nsIURI).spec)
          return;
        Services.obs.removeObserver(arguments.callee, aTopic);
        visited = true;
        maybeCheckResults();
      },
      "uri-visit-saved",
      false
    );
  }

  gTabWaitCount++;
  info("Loading page: " + url);
  tab.linkedBrowser.loadURI(url);
}

function waitForRestoredTab(tab) {
  gTabWaitCount++;

  tab.linkedBrowser.addEventListener("load", function () {
    tab.linkedBrowser.removeEventListener("load", arguments.callee, true);
    if (--gTabWaitCount == 0) {
      ensure_opentabs_match_db(nextStep);
    }
  }, true);
}


function nextStep() {
  if (gTestSteps.length == 0) {
    while (gBrowser.tabs.length > 1) {
      gBrowser.selectTabAtIndex(1);
      gBrowser.removeCurrentTab();
    }

    waitForClearHistory(finish);

    return;
  }

  var stepFunc = gTestSteps.shift();
  stepFunc();
}

function ensure_opentabs_match_db(aCallback) {
  var tabs = {};

  var winEnum = Services.wm.getEnumerator("navigator:browser");
  while (winEnum.hasMoreElements()) {
    let browserWin = winEnum.getNext();
    // skip closed-but-not-destroyed windows
    if (browserWin.closed)
      continue;

    for (let i = 0; i < browserWin.gBrowser.tabContainer.childElementCount; i++) {
      let browser = browserWin.gBrowser.getBrowserAtIndex(i);
      let url = browser.currentURI.spec;
      if (browserWin.isBlankPageURL(url))
        continue;
      if (!(url in tabs))
        tabs[url] = 1;
      else
        tabs[url]++;
    }
  }

  checkAutocompleteResults(tabs, aCallback);
}

/**
 * Clears history invoking callback when done.
 */
function waitForClearHistory(aCallback) {
  const TOPIC_EXPIRATION_FINISHED = "places-expiration-finished";
  let observer = {
    observe: function(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, TOPIC_EXPIRATION_FINISHED);
      aCallback();
    }
  };
  Services.obs.addObserver(observer, TOPIC_EXPIRATION_FINISHED, false);

  PlacesUtils.bhistory.removeAllPages();
}

function checkAutocompleteResults(aExpected, aCallback)
{
  gController.input = {
    timeout: 10,
    textValue: "",
    searches: ["history"],
    searchParam: "enable-actions",
    popupOpen: false,
    minResultsForPopup: 0,
    invalidate: function() {},
    disableAutoComplete: false,
    completeDefaultIndex: false,
    get popup() { return this; },
    onSearchBegin: function() {},
    onSearchComplete:  function ()
    {
      info("Found " + gController.matchCount + " matches.");
      // Check to see the expected uris and titles match up (in any order)
      for (let i = 0; i < gController.matchCount; i++) {
        let uri = gController.getValueAt(i).replace(/^moz-action:[^,]+,/i, "");

        info("Search for '" + uri + "' in open tabs.");
        ok(uri in aExpected, "Registered open page found in autocomplete.");
        // Remove the found entry from expected results.
        delete aExpected[uri];
      }

      // Make sure there is no reported open page that is not open.
      for (let entry in aExpected) {
        ok(false, "'" + entry + "' not found in autocomplete.");
      }

      executeSoon(aCallback);
    },
    setSelectedIndex: function() {},
    get searchCount() { return this.searches.length; },
    getSearchAt: function(aIndex) this.searches[aIndex],
    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsIAutoCompleteInput,
      Ci.nsIAutoCompletePopup,
    ])
  };

  info("Searching open pages.");
  gController.startSearch(Services.prefs.getCharPref("browser.urlbar.restrict.openpage"));
}
