/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  TelemetryTestUtils: "resource://testing-common/TelemetryTestUtils.sys.mjs",
});

const histogramName = "FX_NUMBER_OF_UNIQUE_SITE_ORIGINS_PER_DOCUMENT";
const testRoot = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content",
  "http://mochi.test:8888"
);

function windowGlobalDestroyed(id) {
  return BrowserUtils.promiseObserved(
    "window-global-destroyed",
    aWGP => aWGP.innerWindowId == id
  );
}

async function openAndCloseTab(uri) {
  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: uri,
  });

  const innerWindowId =
    tab.linkedBrowser.browsingContext.currentWindowGlobal.innerWindowId;

  const wgpDestroyed = windowGlobalDestroyed(innerWindowId);
  BrowserTestUtils.removeTab(tab);
  await wgpDestroyed;
}

add_task(async function test_numberOfSiteOriginsAfterTabClose() {
  const histogram = TelemetryTestUtils.getAndClearHistogram(histogramName);
  const testPage = `${testRoot}contain_iframe.html`;

  await openAndCloseTab(testPage);

  // testPage contains two origins: mochi.test:8888 and example.com.
  TelemetryTestUtils.assertHistogram(histogram, 2, 1);
});

add_task(async function test_numberOfSiteOriginsAboutBlank() {
  const histogram = TelemetryTestUtils.getAndClearHistogram(histogramName);

  await openAndCloseTab("about:blank");

  const { values } = histogram.snapshot();
  Assert.deepEqual(
    values,
    {},
    `Histogram should have no values; had ${JSON.stringify(values)}`
  );
});

add_task(async function test_numberOfSiteOriginsMultipleNavigations() {
  const histogram = TelemetryTestUtils.getAndClearHistogram(histogramName);
  const testPage = `${testRoot}contain_iframe.html`;

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: testPage,
    waitForStateStop: true,
  });

  const wgpDestroyedPromises = [
    windowGlobalDestroyed(tab.linkedBrowser.innerWindowID),
  ];

  // Navigate to an interstitial page.
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, "about:blank");
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  // Navigate to another test page.
  BrowserTestUtils.startLoadingURIString(tab.linkedBrowser, testPage);
  await BrowserTestUtils.browserLoaded(tab.linkedBrowser);

  wgpDestroyedPromises.push(
    windowGlobalDestroyed(tab.linkedBrowser.innerWindowID)
  );

  BrowserTestUtils.removeTab(tab);
  await Promise.all(wgpDestroyedPromises);

  // testPage has been loaded twice and contains two origins: mochi.test:8888
  // and example.com.
  TelemetryTestUtils.assertHistogram(histogram, 2, 2);
});

add_task(async function test_numberOfSiteOriginsAddAndRemove() {
  const histogram = TelemetryTestUtils.getAndClearHistogram(histogramName);
  const testPage = `${testRoot}blank_iframe.html`;

  const tab = await BrowserTestUtils.openNewForegroundTab({
    gBrowser,
    opening: testPage,
    waitForStateStop: true,
  });

  // Load a subdocument in the page's iframe.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    const iframe = content.window.document.querySelector("iframe");
    const loaded = new Promise(resolve => {
      iframe.addEventListener("load", () => resolve(), { once: true });
    });
    iframe.src = "http://example.com";

    await loaded;
  });

  // Load a *new* subdocument in the page's iframe. This will result in the page
  // having had three different origins, but only two at any one time.
  await SpecialPowers.spawn(tab.linkedBrowser, [], async () => {
    const iframe = content.window.document.querySelector("iframe");
    const loaded = new Promise(resolve => {
      iframe.addEventListener("load", () => resolve(), { once: true });
    });
    iframe.src = "http://example.org";

    await loaded;
  });

  const wgpDestroyed = windowGlobalDestroyed(tab.linkedBrowser.innerWindowID);
  BrowserTestUtils.removeTab(tab);
  await wgpDestroyed;

  // The page only ever had two origins at once.
  TelemetryTestUtils.assertHistogram(histogram, 2, 1);
});
