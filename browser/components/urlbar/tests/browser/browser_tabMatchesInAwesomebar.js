/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Tests for ensuring that the tab switch results correctly match what is
 * currently available.
 */

requestLongerTimeout(2);

const TEST_URL_BASES = [
  `${TEST_BASE_URL}dummy_page.html#tabmatch`,
  `${TEST_BASE_URL}moz.png#tabmatch`,
];

const RESTRICT_TOKEN_OPENPAGE = "%";

var gTabCounter = 0;

add_task(async function step_1() {
  info("Running step 1");
  let maxResults = Services.prefs.getIntPref("browser.urlbar.maxRichResults");
  let promises = [];
  for (let i = 0; i < maxResults - 1; i++) {
    let tab = BrowserTestUtils.addTab(gBrowser);
    promises.push(loadTab(tab, TEST_URL_BASES[0] + ++gTabCounter));
  }

  await Promise.all(promises);
  await ensure_opentabs_match_db();
});

add_task(async function step_2() {
  info("Running step 2");
  gBrowser.selectTabAtIndex(1);
  gBrowser.removeCurrentTab();
  gBrowser.selectTabAtIndex(1);
  gBrowser.removeCurrentTab();
  gBrowser.selectTabAtIndex(0);

  let promises = [];
  for (let i = 1; i < gBrowser.tabs.length; i++) {
    promises.push(loadTab(gBrowser.tabs[i], TEST_URL_BASES[1] + ++gTabCounter));
  }

  await Promise.all(promises);
  await ensure_opentabs_match_db();
});

add_task(async function step_3() {
  info("Running step 3");
  let promises = [];
  for (let i = 1; i < gBrowser.tabs.length; i++) {
    promises.push(loadTab(gBrowser.tabs[i], TEST_URL_BASES[0] + gTabCounter));
  }

  await Promise.all(promises);
  await ensure_opentabs_match_db();
});

add_task(async function step_4() {
  info("Running step 4 - ensure we don't register subframes as open pages");
  let tab = BrowserTestUtils.addTab(
    gBrowser,
    'data:text/html,<body><iframe src=""></iframe></body>'
  );
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    let iframe_loaded = ContentTaskUtils.waitForEvent(
      content.document,
      "load",
      true
    );
    content.document.querySelector("iframe").src = "http://test2.example.org/";
    await iframe_loaded;
  });

  await ensure_opentabs_match_db();
});

add_task(async function step_5() {
  info("Running step 5 - remove tab immediately");
  let tab = BrowserTestUtils.addTab(gBrowser, "about:logo");
  BrowserTestUtils.removeTab(tab);
  await ensure_opentabs_match_db();
});

add_task(async function step_6() {
  info(
    "Running step 6 - check swapBrowsersAndCloseOther preserves registered switch-to-tab result"
  );
  let tabToKeep = BrowserTestUtils.addTab(gBrowser);
  let tab = BrowserTestUtils.addTab(gBrowser, "about:mozilla");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  gBrowser.updateBrowserRemoteness(tabToKeep.linkedBrowser, {
    remoteType: tab.linkedBrowser.isRemoteBrowser
      ? E10SUtils.DEFAULT_REMOTE_TYPE
      : E10SUtils.NOT_REMOTE,
  });
  gBrowser.swapBrowsersAndCloseOther(tabToKeep, tab);

  await ensure_opentabs_match_db();

  BrowserTestUtils.removeTab(tabToKeep);

  await ensure_opentabs_match_db();
});

add_task(async function step_7() {
  info("Running step 7 - close all tabs");

  Services.prefs.clearUserPref("browser.sessionstore.restore_on_demand");

  BrowserTestUtils.addTab(gBrowser, "about:blank", { skipAnimation: true });
  while (gBrowser.tabs.length > 1) {
    info("Removing tab: " + gBrowser.tabs[0].linkedBrowser.currentURI.spec);
    gBrowser.selectTabAtIndex(0);
    gBrowser.removeCurrentTab();
  }

  await ensure_opentabs_match_db();
});

add_task(async function cleanup() {
  info("Cleaning up");

  await PlacesUtils.history.clear();
});

function loadTab(tab, url) {
  // Because adding visits is async, we will not be notified immediately.
  let loaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  let visited = new Promise(resolve => {
    Services.obs.addObserver(function observer(aSubject, aTopic, aData) {
      if (url != aSubject.QueryInterface(Ci.nsIURI).spec) {
        return;
      }
      Services.obs.removeObserver(observer, aTopic);
      resolve();
    }, "uri-visit-saved");
  });

  info("Loading page: " + url);
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, url);
  return Promise.all([loaded, visited]);
}

function ensure_opentabs_match_db() {
  let tabs = {};

  for (let browserWin of Services.wm.getEnumerator("navigator:browser")) {
    // skip closed-but-not-destroyed windows
    if (browserWin.closed) {
      continue;
    }

    for (let i = 0; i < browserWin.gBrowser.tabs.length; i++) {
      let browser = browserWin.gBrowser.getBrowserAtIndex(i);
      let url = browser.currentURI.spec;
      if (browserWin.isBlankPageURL(url)) {
        continue;
      }
      if (!(url in tabs)) {
        tabs[url] = 1;
      } else {
        tabs[url]++;
      }
    }
  }

  return checkAutocompleteResults(tabs);
}

async function checkAutocompleteResults(expected) {
  info("Searching open pages.");
  await UrlbarTestUtils.promiseAutocompleteResultPopup({
    window,
    value: RESTRICT_TOKEN_OPENPAGE,
  });

  let resultCount = UrlbarTestUtils.getResultCount(window);
  for (let i = 0; i < resultCount; i++) {
    let result = await UrlbarTestUtils.getDetailsOfResultAt(window, i);
    if (result.heuristic) {
      info("Skip heuristic match");
      continue;
    }

    Assert.equal(
      result.type,
      UrlbarUtils.RESULT_TYPE.TAB_SWITCH,
      "Should have a tab switch result"
    );

    let url = result.url;

    info(`Search for ${url} in open tabs.`);
    let inExpected = url in expected;
    Assert.ok(
      inExpected,
      `${url} was found in autocomplete, was ${
        inExpected ? "" : "not "
      } expected`
    );
    // Remove the found entry from expected results.
    delete expected[url];
  }

  await UrlbarTestUtils.promisePopupClose(window, () =>
    EventUtils.synthesizeKey("KEY_Escape")
  );

  // Make sure there is no reported open page that is not open.
  for (let entry in expected) {
    Assert.ok(!entry, `Should have been found in autocomplete`);
  }
}
