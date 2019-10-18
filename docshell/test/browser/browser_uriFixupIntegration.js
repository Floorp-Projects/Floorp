/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { UrlbarTestUtils } = ChromeUtils.import(
  "resource://testing-common/UrlbarTestUtils.jsm"
);

const kSearchEngineID = "browser_urifixup_search_engine";
const kSearchEngineURL = "http://example.com/?search={searchTerms}";
const kPrivateSearchEngineID = "browser_urifixup_search_engine_private";
const kPrivateSearchEngineURL = "http://example.com/?private={searchTerms}";

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.search.separatePrivateDefault.ui.enabled", true],
      ["browser.search.separatePrivateDefault", true],
    ],
  });

  let oldCurrentEngine = await Services.search.getDefault();
  let oldPrivateEngine = await Services.search.getDefaultPrivate();

  // Add new fake search engines.
  let newCurrentEngine = await Services.search.addEngineWithDetails(
    kSearchEngineID,
    {
      method: "get",
      template: kSearchEngineURL,
    }
  );
  await Services.search.setDefault(newCurrentEngine);

  let newPrivateEngine = await Services.search.addEngineWithDetails(
    kPrivateSearchEngineID,
    {
      method: "get",
      template: kPrivateSearchEngineURL,
    }
  );
  await Services.search.setDefaultPrivate(newPrivateEngine);

  // Remove the fake engines when done.
  registerCleanupFunction(async () => {
    if (oldCurrentEngine) {
      await Services.search.setDefault(oldCurrentEngine);
    }
    if (oldPrivateEngine) {
      await Services.search.setDefault(oldPrivateEngine);
    }
    await Services.search.removeEngine(newCurrentEngine);
    await Services.search.removeEngine(newPrivateEngine);
  });
});

add_task(async function test() {
  // Test both directly setting a value and pressing enter, or setting the
  // value through input events, like the user would do.
  const setValueFns = [
    (value, win) => {
      win.gURLBar.value = value;
    },
    (value, win) => {
      return UrlbarTestUtils.promiseAutocompleteResultPopup({
        window: win,
        waitForFocus: SimpleTest.waitForFocus,
        value,
      });
    },
  ];

  for (let value of ["foo bar", "brokenprotocol:somethingelse"]) {
    for (let setValueFn of setValueFns) {
      for (let inPrivateWindow of [false, true]) {
        await do_test(value, setValueFn, inPrivateWindow);
      }
    }
  }
});

async function do_test(value, setValueFn, inPrivateWindow) {
  info(`Search ${value} in a ${inPrivateWindow ? "private" : "normal"} window`);
  let win = await BrowserTestUtils.openNewBrowserWindow({
    private: inPrivateWindow,
  });
  // Enter search terms and start a search.
  win.gURLBar.focus();
  await setValueFn(value, win);

  EventUtils.synthesizeKey("KEY_Enter", {}, win);

  // Check that we load the correct URL.
  let escapedValue = encodeURIComponent(value).replace("%20", "+");
  let searchEngineUrl = inPrivateWindow
    ? kPrivateSearchEngineURL
    : kSearchEngineURL;
  let expectedURL = searchEngineUrl.replace("{searchTerms}", escapedValue);
  await BrowserTestUtils.browserLoaded(
    win.gBrowser.selectedBrowser,
    false,
    expectedURL
  );
  // There should be at least one test.
  Assert.equal(
    win.gBrowser.selectedBrowser.currentURI.spec,
    expectedURL,
    "New tab should have loaded with expected url."
  );

  // Cleanup.
  await BrowserTestUtils.closeWindow(win);
}
