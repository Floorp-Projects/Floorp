/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URL = `${TEST_BASE_URL}file_blank_but_not_blank.html`;

add_task(async function () {
  for (let page of gInitialPages) {
    if (page == "about:newtab") {
      // New tab preloading makes this a pain to test, so skip
      continue;
    }
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, page);
    ok(
      !gURLBar.value,
      "The URL bar should be empty if we load a plain " + page + " page."
    );
    BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function () {
  // The test was originally to check that reloading of a javascript: URL could
  // throw an error and empty the URL bar. This situation can no longer happen
  // as in bug 836567 we set document.URL to active document's URL on navigation
  // to a javascript: URL; reloading after that will simply reload the original
  // active document rather than the javascript: URL itself. But we can still
  // verify that the URL bar's value is correct.
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  is(
    gURLBar.value,
    UrlbarTestUtils.trimURL(TEST_URL),
    "The URL bar should match the URI"
  );
  let browserLoaded = BrowserTestUtils.browserLoaded(tab.linkedBrowser);
  SpecialPowers.spawn(tab.linkedBrowser, [], function () {
    content.document.querySelector("a").click();
  });
  await browserLoaded;
  is(
    gURLBar.value,
    UrlbarTestUtils.trimURL(TEST_URL),
    "The URL bar should be the previous active document's URI."
  );
  await SpecialPowers.spawn(tab.linkedBrowser, [], async function () {
    // This is sync, so by the time we return we should have changed the URL bar.
    content.location.reload();
  }).catch(e => {
    // Ignore expected exception.
  });
  is(
    gURLBar.value,
    UrlbarTestUtils.trimURL(TEST_URL),
    "The URL bar should still be the previous active document's URI."
  );
  BrowserTestUtils.removeTab(tab);
});
