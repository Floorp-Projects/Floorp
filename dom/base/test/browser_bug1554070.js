"use strict";

const HTTPS_TEST_ROOT = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "https://example.com"
);

const URL0 = HTTPS_TEST_ROOT + "file_bug1554070_1.html";
const URL1 = HTTPS_TEST_ROOT + "file_bug1554070_2.html";
const URL2 = "https://example.org/";

add_task(async function () {
  let tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    waitForLoad: true,
  });

  let browser = tab.linkedBrowser;

  function click() {
    return SpecialPowers.spawn(browser, [], () => {
      let anchor = content.document.querySelector("a");
      anchor.click();
    });
  }

  // Load file_bug1554070_1.html.
  BrowserTestUtils.startLoadingURIString(browser, URL0);
  await BrowserTestUtils.browserLoaded(browser, false, URL0);
  is(gBrowser.currentURI.spec, URL0, "loaded file_bug1554070_1.html");

  // Click the link in file_bug1554070_1.html. It should open
  // file_bug1554070_2.html in the current tab.
  await click();
  await BrowserTestUtils.browserLoaded(browser, false, URL1);
  is(gBrowser.currentURI.spec, URL1, "loaded file_bug1554070_2.html");

  // Click the link in file_bug1554070_2.html. It should open example.org in
  // a new tab.
  await Promise.all([
    click(),
    BrowserTestUtils.waitForNewTab(gBrowser, URL2, true),
  ]);
  is(gBrowser.tabs.length, 3, "got new tab");
  is(gBrowser.currentURI.spec, URL2, "loaded example.org");

  BrowserTestUtils.removeTab(gBrowser.selectedTab);
  BrowserTestUtils.removeTab(tab);
});
