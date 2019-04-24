/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests to ensure that Activity Stream loads in the privileged content process.
 * Normal http web pages should load in the web content process.
 * Ref: Bug 1469072.
 */

const ABOUT_BLANK = "about:blank";
const ABOUT_HOME = "about:home";
const ABOUT_NEWTAB = "about:newtab";
const ABOUT_WELCOME = "about:welcome";
const TEST_HTTP = "http://example.org/";

/**
 * Takes a xul:browser and makes sure that the remoteTypes for the browser in
 * both the parent and the child processes are the same.
 *
 * @param {xul:browser} browser
 *        A xul:browser.
 * @param {string} expectedRemoteType
 *        The expected remoteType value for the browser in both the parent
 *        and child processes.
 * @param {optional string} message
 *        If provided, shows this string as the message when remoteType values
 *        do not match. If not present, it uses the default message defined
 *        in the function parameters.
 */
function checkBrowserRemoteType(
  browser,
  expectedRemoteType,
  message = `Ensures that tab runs in the ${expectedRemoteType} content process.`
) {
  // Check both parent and child to ensure that they have the correct remoteType.
  is(browser.remoteType, expectedRemoteType, message);
  is(browser.messageManager.remoteType, expectedRemoteType,
    "Parent and child process should agree on the remote type.");
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["browser.newtab.preload", false],
      ["browser.tabs.remote.separatePrivilegedContentProcess", true],
      ["dom.ipc.processCount.privileged", 1],
      ["dom.ipc.keepProcessesAlive.privileged", 1],
    ],
  });
});

/*
 * Test to ensure that the Activity Stream tabs open in privileged content
 * process. We will first open an about:newtab page that acts as a reference to
 * the privileged content process. With the reference, we can then open Activity
 * Stream links in a new tab and ensure that the new tab opens in the same
 * privileged content process as our reference.
 */
add_task(async function activity_stream_in_privileged_content_process() {
  Services.ppmm.releaseCachedProcesses();

  await BrowserTestUtils.withNewTab(ABOUT_NEWTAB, async function(browser1) {
    checkBrowserRemoteType(browser1, E10SUtils.PRIVILEGED_REMOTE_TYPE);

    // Note the processID for about:newtab for comparison later.
    let privilegedPid = browser1.frameLoader.remoteTab.osPid;

    for (let url of [
      ABOUT_NEWTAB,
      ABOUT_WELCOME,
      ABOUT_HOME,
      `${ABOUT_NEWTAB}#foo`,
      `${ABOUT_WELCOME}#bar`,
      `${ABOUT_HOME}#baz`,
      `${ABOUT_NEWTAB}?q=foo`,
      `${ABOUT_WELCOME}?q=bar`,
      `${ABOUT_HOME}?q=baz`,
    ]) {
      await BrowserTestUtils.withNewTab(url, async function(browser2) {
        is(browser2.frameLoader.remoteTab.osPid, privilegedPid,
          "Check that about:newtab tabs are in the same privileged content process.");
      });
    }
  });

  Services.ppmm.releaseCachedProcesses();
});

/*
 * Test to ensure that a process switch occurs when navigating between normal
 * web pages and Activity Stream pages in the same tab.
 */
add_task(async function process_switching_through_loading_in_the_same_tab() {
  Services.ppmm.releaseCachedProcesses();

  await BrowserTestUtils.withNewTab(TEST_HTTP, async function(browser) {
    checkBrowserRemoteType(browser, E10SUtils.WEB_REMOTE_TYPE);

    for (let [url, remoteType] of [
      [ABOUT_NEWTAB, E10SUtils.PRIVILEGED_REMOTE_TYPE],
      [ABOUT_BLANK, E10SUtils.PRIVILEGED_REMOTE_TYPE],
      [TEST_HTTP, E10SUtils.WEB_REMOTE_TYPE],
      [ABOUT_HOME, E10SUtils.PRIVILEGED_REMOTE_TYPE],
      [TEST_HTTP, E10SUtils.WEB_REMOTE_TYPE],
      [ABOUT_WELCOME, E10SUtils.PRIVILEGED_REMOTE_TYPE],
      [TEST_HTTP, E10SUtils.WEB_REMOTE_TYPE],
      [ABOUT_BLANK, E10SUtils.WEB_REMOTE_TYPE],
      [`${ABOUT_NEWTAB}#foo`, E10SUtils.PRIVILEGED_REMOTE_TYPE],
      [TEST_HTTP, E10SUtils.WEB_REMOTE_TYPE],
      [`${ABOUT_WELCOME}#bar`, E10SUtils.PRIVILEGED_REMOTE_TYPE],
      [TEST_HTTP, E10SUtils.WEB_REMOTE_TYPE],
      [`${ABOUT_HOME}#baz`, E10SUtils.PRIVILEGED_REMOTE_TYPE],
      [TEST_HTTP, E10SUtils.WEB_REMOTE_TYPE],
      [`${ABOUT_NEWTAB}?q=foo`, E10SUtils.PRIVILEGED_REMOTE_TYPE],
      [TEST_HTTP, E10SUtils.WEB_REMOTE_TYPE],
      [`${ABOUT_WELCOME}?q=bar`, E10SUtils.PRIVILEGED_REMOTE_TYPE],
      [TEST_HTTP, E10SUtils.WEB_REMOTE_TYPE],
      [`${ABOUT_HOME}?q=baz`, E10SUtils.PRIVILEGED_REMOTE_TYPE],
      [TEST_HTTP, E10SUtils.WEB_REMOTE_TYPE],
    ]) {
      BrowserTestUtils.loadURI(browser, url);
      await BrowserTestUtils.browserLoaded(browser, false, url);
      checkBrowserRemoteType(browser, remoteType);
    }
  });

  Services.ppmm.releaseCachedProcesses();
});

/*
 * Test to ensure that a process switch occurs when navigating between normal
 * web pages and Activity Stream pages using the browser's navigation features
 * such as history and location change.
 */
add_task(async function process_switching_through_navigation_features() {
  Services.ppmm.releaseCachedProcesses();

  await BrowserTestUtils.withNewTab(ABOUT_NEWTAB, async function(browser) {
    checkBrowserRemoteType(browser, E10SUtils.PRIVILEGED_REMOTE_TYPE);

    // Note the processID for about:newtab for comparison later.
    let privilegedPid = browser.frameLoader.remoteTab.osPid;

    // Check that about:newtab opened from JS in about:newtab page is in the same process.
    let promiseTabOpened = BrowserTestUtils.waitForNewTab(gBrowser, ABOUT_NEWTAB, true);
    await ContentTask.spawn(browser, ABOUT_NEWTAB, uri => {
      content.open(uri, "_blank");
    });
    let newTab = await promiseTabOpened;
    registerCleanupFunction(async function() {
      BrowserTestUtils.removeTab(newTab);
    });
    browser = newTab.linkedBrowser;
    is(browser.frameLoader.remoteTab.osPid, privilegedPid,
      "Check that new tab opened from about:newtab is loaded in privileged content process.");

    // Check that reload does not break the privileged content process affinity.
    BrowserReload();
    await BrowserTestUtils.browserLoaded(browser, false, ABOUT_NEWTAB);
    is(browser.frameLoader.remoteTab.osPid, privilegedPid,
      "Check that about:newtab is still in privileged content process after reload.");

    // Load http webpage
    BrowserTestUtils.loadURI(browser, TEST_HTTP);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_HTTP);
    checkBrowserRemoteType(browser, E10SUtils.WEB_REMOTE_TYPE);

    // Check that using the history back feature switches back to privileged content process.
    let promiseLocation = BrowserTestUtils.waitForLocationChange(gBrowser, ABOUT_NEWTAB);
    browser.goBack();
    await promiseLocation;
    // We will need to ensure that the process flip has fully completed so that
    // the navigation history data will be available when we do browser.goForward();
    await BrowserTestUtils.waitForEvent(newTab, "SSTabRestored");
    is(browser.frameLoader.remoteTab.osPid, privilegedPid,
      "Check that about:newtab is still in privileged content process after history goBack.");

    // Check that using the history forward feature switches back to the web content process.
    promiseLocation = BrowserTestUtils.waitForLocationChange(gBrowser, TEST_HTTP);
    browser.goForward();
    await promiseLocation;
    // We will need to ensure that the process flip has fully completed so that
    // the navigation history data will be available when we do browser.gotoIndex(0);
    await BrowserTestUtils.waitForEvent(newTab, "SSTabRestored");
    checkBrowserRemoteType(browser, E10SUtils.WEB_REMOTE_TYPE,
      "Check that tab runs in the web content process after using history goForward.");

    // Check that goto history index does not break the affinity.
    promiseLocation = BrowserTestUtils.waitForLocationChange(gBrowser, ABOUT_NEWTAB);
    browser.gotoIndex(0);
    await promiseLocation;
    is(browser.frameLoader.remoteTab.osPid, privilegedPid,
      "Check that about:newtab is in privileged content process after history gotoIndex.");

    BrowserTestUtils.loadURI(browser, TEST_HTTP);
    await BrowserTestUtils.browserLoaded(browser, false, TEST_HTTP);
    checkBrowserRemoteType(browser, E10SUtils.WEB_REMOTE_TYPE);

    // Check that location change causes a change in process type as well.
    await ContentTask.spawn(browser, ABOUT_NEWTAB, uri => {
      content.location = uri;
    });
    await BrowserTestUtils.browserLoaded(browser, false, ABOUT_NEWTAB);
    is(browser.frameLoader.remoteTab.osPid, privilegedPid,
      "Check that about:newtab is in privileged content process after location change.");
  });

  Services.ppmm.releaseCachedProcesses();
});
