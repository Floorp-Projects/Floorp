/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

requestLongerTimeout(2);

const TEST_URL_BASES = [
  "http://example.org/browser/browser/base/content/test/urlbar/dummy_page.html#tabmatch",
  "http://example.org/browser/browser/base/content/test/urlbar/moz.png#tabmatch"
];

var gController = Cc["@mozilla.org/autocomplete/controller;1"].
                  getService(Ci.nsIAutoCompleteController);

var gTabCounter = 0;

add_task(function* step_1() {
  info("Running step 1");
  let maxResults = Services.prefs.getIntPref("browser.urlbar.maxRichResults");
  let promises = [];
  for (let i = 0; i < maxResults - 1; i++) {
    let tab = gBrowser.addTab();
    promises.push(loadTab(tab, TEST_URL_BASES[0] + (++gTabCounter)));
  }

  yield Promise.all(promises);
  yield ensure_opentabs_match_db();
});

add_task(function* step_2() {
  info("Running step 2");
  gBrowser.selectTabAtIndex(1);
  gBrowser.removeCurrentTab();
  gBrowser.selectTabAtIndex(1);
  gBrowser.removeCurrentTab();

  let promises = [];
  for (let i = 1; i < gBrowser.tabs.length; i++)
    promises.push(loadTab(gBrowser.tabs[i], TEST_URL_BASES[1] + (++gTabCounter)));

  yield Promise.all(promises);
  yield ensure_opentabs_match_db();
});

add_task(function* step_3() {
  info("Running step 3");
  let promises = [];
  for (let i = 1; i < gBrowser.tabs.length; i++)
    promises.push(loadTab(gBrowser.tabs[i], TEST_URL_BASES[0] + gTabCounter));

  yield Promise.all(promises);
  yield ensure_opentabs_match_db();
});

add_task(function* step_4() {
  info("Running step 4 - ensure we don't register subframes as open pages");
  let tab = gBrowser.addTab();
  tab.linkedBrowser.loadURI('data:text/html,<body><iframe src=""></iframe></body>');
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  yield ContentTask.spawn(tab.linkedBrowser, null, function* () {
    let iframe_loaded = ContentTaskUtils.waitForEvent(content.document, "load", true);
    content.document.querySelector("iframe").src = "http://test2.example.org/";
    yield iframe_loaded;
  });

  yield ensure_opentabs_match_db();
});

add_task(function* step_5() {
  info("Running step 5 - remove tab immediately");
  let tab = gBrowser.addTab("about:logo");
  yield BrowserTestUtils.removeTab(tab);
  yield ensure_opentabs_match_db();
});

add_task(function* step_6() {
  info("Running step 6 - check swapBrowsersAndCloseOther preserves registered switch-to-tab result");
  let tabToKeep = gBrowser.addTab();
  let tab = gBrowser.addTab();
  tab.linkedBrowser.loadURI("about:mozilla");
  yield BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  gBrowser.updateBrowserRemoteness(tabToKeep.linkedBrowser, tab.linkedBrowser.isRemoteBrowser);
  gBrowser.swapBrowsersAndCloseOther(tabToKeep, tab);

  yield ensure_opentabs_match_db()

  yield BrowserTestUtils.removeTab(tabToKeep);

  yield ensure_opentabs_match_db();
});

add_task(function* step_7() {
  info("Running step 7 - close all tabs");

  Services.prefs.clearUserPref("browser.sessionstore.restore_on_demand");

  gBrowser.addTab("about:blank", {skipAnimation: true});
  while (gBrowser.tabs.length > 1) {
    info("Removing tab: " + gBrowser.tabs[0].linkedBrowser.currentURI.spec);
    gBrowser.selectTabAtIndex(0);
    gBrowser.removeCurrentTab();
  }

  yield ensure_opentabs_match_db();
});

add_task(function* cleanup() {
  info("Cleaning up");

  yield PlacesTestUtils.clearHistory();
});

function loadTab(tab, url) {
  // Because adding visits is async, we will not be notified immediately.
  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  let visited = new Promise(resolve => {
    Services.obs.addObserver(
      function observer(aSubject, aTopic, aData) {
        if (url != aSubject.QueryInterface(Ci.nsIURI).spec)
          return;
        Services.obs.removeObserver(observer, aTopic);
        resolve();
      },
      "uri-visit-saved",
      false
    );
  });

  info("Loading page: " + url);
  tab.linkedBrowser.loadURI(url);
  return Promise.all([ loaded, visited ]);
}

function ensure_opentabs_match_db() {
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

  return new Promise(resolve => {
    checkAutocompleteResults(tabs, resolve);
  });
}

function checkAutocompleteResults(aExpected, aCallback)
{
  gController.input = {
    timeout: 10,
    textValue: "",
    searches: ["unifiedcomplete"],
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
        if (gController.getStyleAt(i).includes("heuristic")) {
          info("Skip heuristic match");
          continue;
        }
        let action = gURLBar.popup.input._parseActionUrl(gController.getValueAt(i));
        let uri = action.params.url;

        info("Search for '" + uri + "' in open tabs.");
        let expected = uri in aExpected;
        ok(expected, uri + " was found in autocomplete, was " + (expected ? "" : "not ") + "expected");
        // Remove the found entry from expected results.
        delete aExpected[uri];
      }

      // Make sure there is no reported open page that is not open.
      for (let entry in aExpected) {
        ok(false, "'" + entry + "' should be found in autocomplete");
      }

      executeSoon(aCallback);
    },
    setSelectedIndex: function() {},
    get searchCount() { return this.searches.length; },
    getSearchAt: function(aIndex) { return this.searches[aIndex]; },
    QueryInterface: XPCOMUtils.generateQI([
      Ci.nsIAutoCompleteInput,
      Ci.nsIAutoCompletePopup,
    ])
  };

  info("Searching open pages.");
  gController.startSearch(Services.prefs.getCharPref("browser.urlbar.restrict.openpage"));
}
