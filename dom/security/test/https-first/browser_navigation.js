"use strict";

const REQUEST_URL =
  "http://httpsfirst.com/browser/dom/security/test/https-first/";

async function promiseGetHistoryIndex(browser) {
  if (!SpecialPowers.Services.appinfo.sessionHistoryInParent) {
    return SpecialPowers.spawn(browser, [], function() {
      let shistory =
        docShell.browsingContext.childSessionHistory.legacySHistory;
      return shistory.index;
    });
  }

  let shistory = browser.browsingContext.sessionHistory;
  return shistory.index;
}

async function testNavigations() {
  // Load initial site
  let loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.loadURI(gBrowser, REQUEST_URL + "file_navigation.html?foo1");
  await loaded;

  // Load another site
  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [REQUEST_URL],
    async function(url) {
      content.location.href = url + "file_navigation.html?foo2";
    }
  );
  await loaded;

  // Load another site
  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [REQUEST_URL],
    async function(url) {
      content.location.href = url + "file_navigation.html?foo3";
    }
  );
  await loaded;
  is(
    await promiseGetHistoryIndex(gBrowser.selectedBrowser),
    2,
    "correct session history index after load 3"
  );

  // Go back one site by clicking the back button
  info("Clicking back button");
  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  let backButton = document.getElementById("back-button");
  backButton.click();
  await loaded;
  is(
    await promiseGetHistoryIndex(gBrowser.selectedBrowser),
    1,
    "correct session history index after going back for the first time"
  );

  // Go back again
  info("Clicking back button again");
  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  backButton.click();
  await loaded;
  is(
    await promiseGetHistoryIndex(gBrowser.selectedBrowser),
    0,
    "correct session history index after going back for the second time"
  );
}

add_task(async function() {
  waitForExplicitFinish();

  await SpecialPowers.pushPrefEnv({
    set: [["dom.security.https_first", true]],
  });

  await testNavigations();

  // If fission is enabled we also want to test the navigations with the bfcache
  // in the parent.
  if (SpecialPowers.getBoolPref("fission.autostart")) {
    await SpecialPowers.pushPrefEnv({
      set: [["fission.bfcacheInParent", true]],
    });

    await testNavigations();
  }

  finish();
});
