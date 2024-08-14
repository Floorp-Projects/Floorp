/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const FocusManager = Services.focus;

const SITE_URL_1 =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "https://example.com"
  ) + "empty.html";

const SITE_URL_2 =
  getRootDirectory(gTestPath).replace(
    "chrome://mochitests/content",
    "http://127.0.0.1:8888"
  ) + "empty.html";

// Test ensures that when a page goes to BFCache, it won't
// accidentally update the active browsing context to null to
// the parent process.
add_task(async function () {
  // Load Site 1
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, SITE_URL_1);

  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, SITE_URL_2);
  // Navigated to Site 2 in the same tab
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  const pageNavigatedBackToSite1 = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "pageshow"
  );
  tab.linkedBrowser.goBack();
  // Navigated site 1 by going back
  await pageNavigatedBackToSite1;

  const pageHideForSite1Run = SpecialPowers.spawn(tab.linkedBrowser, [], () => {
    return new Promise(r => {
      content.addEventListener("pagehide", function () {
        const start = Date.now();
        // block the main thread for 2 seconds.
        while (Date.now() - start < 2000) {
          r();
        }
      });
    });
  });

  let pageNavigatedBackToSite2 = BrowserTestUtils.waitForContentEvent(
    tab.linkedBrowser,
    "pageshow"
  );

  // Navigate to site 2 again by going forward.
  //
  // In a buggy Firefox build, this navigation would trigger
  // two activeBrowsingContextInChrome updates. One from
  // site 1 to set it to nullptr, and one from the site 2
  // to itself.
  tab.linkedBrowser.goForward();

  await pageNavigatedBackToSite2;
  // Forcefully to make site 1 to update activeBrowsingContextInChrome
  // after site 2.
  await pageHideForSite1Run;

  // Give the parent process some opportunities to update
  // the activeBrowsingContextInChrome via IPC.
  await new Promise(r => {
    /* eslint-disable mozilla/no-arbitrary-setTimeout */
    setTimeout(r, 2000);
  });

  Assert.ok(
    !!FocusManager.activeContentBrowsingContext,
    "active browsing context in content should be non-null"
  );
  BrowserTestUtils.removeTab(tab);
});
