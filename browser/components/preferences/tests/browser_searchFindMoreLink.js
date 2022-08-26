/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const TEST_URL = "https://example.org/";

add_setup(async () => {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.search.searchEnginesURL", TEST_URL]],
  });
});

add_task(async function test_click_find_more_link() {
  await openPreferencesViaOpenPreferencesAPI("search", { leaveOpen: true });

  let promiseNewTab = BrowserTestUtils.waitForNewTab(gBrowser);

  gBrowser.selectedBrowser.contentDocument
    .getElementById("addEngines")
    .scrollIntoView();
  await BrowserTestUtils.synthesizeMouseAtCenter(
    "#addEngines",
    {},
    gBrowser.selectedBrowser.browsingContext
  );

  let tab = await promiseNewTab;
  Assert.equal(
    tab.linkedBrowser.documentURI.spec,
    TEST_URL,
    "Should have loaded the expected page"
  );

  // Close both tabs.
  gBrowser.removeCurrentTab();
  gBrowser.removeCurrentTab();
});
