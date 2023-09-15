/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests to ensure that Mozilla Privileged Webpages load in the privileged
 * mozilla web content process. Normal http web pages should load in the web
 * content process.
 * Ref: Bug 1539595.
 */

// High and Low Privilege
const TEST_HIGH1 = "https://example.org/";
const TEST_HIGH2 = "https://test1.example.org/";
// eslint-disable-next-line @microsoft/sdl/no-insecure-url
const TEST_LOW1 = "http://example.org/";
const TEST_LOW2 = "https://example.com/";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.tabs.remote.separatePrivilegedMozillaWebContentProcess", true],
      ["browser.tabs.remote.separatedMozillaDomains", "example.org"],
      ["dom.ipc.processCount.privilegedmozilla", 1],
    ],
  });
});

/*
 * Test to ensure that the tabs open in privileged mozilla content process. We
 * will first open a page that acts as a reference to the privileged mozilla web
 * content process. With the reference, we can then open other links in a new tab
 * and ensure that the new tab opens in the same privileged mozilla content process
 * as our reference.
 */
add_task(async function webpages_in_privileged_content_process() {
  Services.ppmm.releaseCachedProcesses();

  await BrowserTestUtils.withNewTab(TEST_HIGH1, async function (browser1) {
    checkBrowserRemoteType(browser1, E10SUtils.PRIVILEGEDMOZILLA_REMOTE_TYPE);

    // Note the processID for about:newtab for comparison later.
    let privilegedPid = browser1.frameLoader.remoteTab.osPid;

    for (let url of [
      TEST_HIGH1,
      `${TEST_HIGH1}#foo`,
      `${TEST_HIGH1}?q=foo`,
      TEST_HIGH2,
      `${TEST_HIGH2}#foo`,
      `${TEST_HIGH2}?q=foo`,
    ]) {
      await BrowserTestUtils.withNewTab(url, async function (browser2) {
        is(
          browser2.frameLoader.remoteTab.osPid,
          privilegedPid,
          "Check that privileged pages are in the same privileged mozilla content process."
        );
      });
    }
  });

  Services.ppmm.releaseCachedProcesses();
});

/*
 * Test to ensure that a process switch occurs when navigating between normal
 * web pages and unprivileged pages in the same tab.
 */
add_task(async function process_switching_through_loading_in_the_same_tab() {
  Services.ppmm.releaseCachedProcesses();

  await BrowserTestUtils.withNewTab(TEST_LOW1, async function (browser) {
    checkBrowserRemoteType(browser, E10SUtils.WEB_REMOTE_TYPE);

    for (let [url, remoteType] of [
      [TEST_HIGH1, E10SUtils.PRIVILEGEDMOZILLA_REMOTE_TYPE],
      [TEST_LOW1, E10SUtils.WEB_REMOTE_TYPE],
      [TEST_HIGH1, E10SUtils.PRIVILEGEDMOZILLA_REMOTE_TYPE],
      [TEST_LOW2, E10SUtils.WEB_REMOTE_TYPE],
      [TEST_HIGH1, E10SUtils.PRIVILEGEDMOZILLA_REMOTE_TYPE],
      [TEST_LOW1, E10SUtils.WEB_REMOTE_TYPE],
      [TEST_LOW2, E10SUtils.WEB_REMOTE_TYPE],
      [`${TEST_HIGH1}#foo`, E10SUtils.PRIVILEGEDMOZILLA_REMOTE_TYPE],
      [TEST_LOW1, E10SUtils.WEB_REMOTE_TYPE],
      [`${TEST_HIGH1}#bar`, E10SUtils.PRIVILEGEDMOZILLA_REMOTE_TYPE],
      [TEST_LOW2, E10SUtils.WEB_REMOTE_TYPE],
      [`${TEST_HIGH1}#baz`, E10SUtils.PRIVILEGEDMOZILLA_REMOTE_TYPE],
      [TEST_LOW1, E10SUtils.WEB_REMOTE_TYPE],
      [`${TEST_HIGH1}?q=foo`, E10SUtils.PRIVILEGEDMOZILLA_REMOTE_TYPE],
      [TEST_LOW2, E10SUtils.WEB_REMOTE_TYPE],
      [`${TEST_HIGH1}?q=bar`, E10SUtils.PRIVILEGEDMOZILLA_REMOTE_TYPE],
      [TEST_LOW1, E10SUtils.WEB_REMOTE_TYPE],
      [`${TEST_HIGH1}?q=baz`, E10SUtils.PRIVILEGEDMOZILLA_REMOTE_TYPE],
      [TEST_LOW2, E10SUtils.WEB_REMOTE_TYPE],
    ]) {
      BrowserTestUtils.startLoadingURIString(browser, url);
      await BrowserTestUtils.browserLoaded(browser, false, url);
      checkBrowserRemoteType(browser, remoteType);
    }
  });

  Services.ppmm.releaseCachedProcesses();
});

/*
 * Test to ensure that a process switch occurs when navigating between normal
 * web pages and privileged pages using the browser's navigation features
 * such as history and location change.
 */
add_task(async function process_switching_through_navigation_features() {
  Services.ppmm.releaseCachedProcesses();

  await BrowserTestUtils.withNewTab(TEST_HIGH1, async function (browser) {
    checkBrowserRemoteType(browser, E10SUtils.PRIVILEGEDMOZILLA_REMOTE_TYPE);

    // Note the processID for about:newtab for comparison later.
    let privilegedPid = browser.frameLoader.remoteTab.osPid;

    // Check that about:newtab opened from JS in about:newtab page is in the same process.
    let promiseTabOpened = BrowserTestUtils.waitForNewTab(
      gBrowser,
      TEST_HIGH1,
      true
    );
    await SpecialPowers.spawn(browser, [TEST_HIGH1], uri => {
      content.open(uri, "_blank");
    });
    let newTab = await promiseTabOpened;
    registerCleanupFunction(async function () {
      BrowserTestUtils.removeTab(newTab);
    });
    browser = newTab.linkedBrowser;
    is(
      browser.frameLoader.remoteTab.osPid,
      privilegedPid,
      "Check that new tab opened from privileged page is loaded in privileged mozilla content process."
    );

    // Check that reload does not break the privileged mozilla content process affinity.
    BrowserReload();
    await BrowserTestUtils.browserLoaded(browser, false, TEST_HIGH1);
    is(
      browser.frameLoader.remoteTab.osPid,
      privilegedPid,
      "Check that privileged page is still in privileged mozilla content process after reload."
    );

    // Load http webpage
    BrowserTestUtils.startLoadingURIString(browser, TEST_LOW1);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_LOW1);
    checkBrowserRemoteType(browser, E10SUtils.WEB_REMOTE_TYPE);

    // Check that using the history back feature switches back to privileged mozilla content process.
    let promiseLocation = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_HIGH1
    );
    browser.goBack();
    await promiseLocation;
    is(
      browser.frameLoader.remoteTab.osPid,
      privilegedPid,
      "Check that privileged page is still in privileged mozilla content process after history goBack."
    );

    // Check that using the history forward feature switches back to the web content process.
    promiseLocation = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_LOW1
    );
    browser.goForward();
    await promiseLocation;
    checkBrowserRemoteType(
      browser,
      E10SUtils.WEB_REMOTE_TYPE,
      "Check that tab runs in the web content process after using history goForward."
    );

    // Check that goto history index does not break the affinity.
    promiseLocation = BrowserTestUtils.waitForLocationChange(
      gBrowser,
      TEST_HIGH1
    );
    browser.gotoIndex(0);
    await promiseLocation;
    is(
      browser.frameLoader.remoteTab.osPid,
      privilegedPid,
      "Check that privileged page is in privileged mozilla content process after history gotoIndex."
    );

    BrowserTestUtils.startLoadingURIString(browser, TEST_LOW2);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_LOW2);
    checkBrowserRemoteType(browser, E10SUtils.WEB_REMOTE_TYPE);

    // Check that location change causes a change in process type as well.
    await SpecialPowers.spawn(browser, [TEST_HIGH1], uri => {
      content.location = uri;
    });
    await BrowserTestUtils.browserLoaded(browser, false, TEST_HIGH1);
    is(
      browser.frameLoader.remoteTab.osPid,
      privilegedPid,
      "Check that privileged page is in privileged mozilla content process after location change."
    );
  });

  Services.ppmm.releaseCachedProcesses();
});
