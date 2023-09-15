"use strict";

const REQUEST_URL =
  "https://example.com/browser/dom/security/test/sec-fetch/file_no_cache.sjs";

let gTestCounter = 0;
let gExpectedHeader = {};

async function setup() {
  waitForExplicitFinish();
}

function checkSecFetchUser(subject, topic, data) {
  let channel = subject.QueryInterface(Ci.nsIHttpChannel);
  if (!channel.URI.spec.startsWith("https://example.com/")) {
    return;
  }

  info(`testing headers for load of ${channel.URI.spec}`);

  const secFetchHeaders = [
    "sec-fetch-mode",
    "sec-fetch-dest",
    "sec-fetch-user",
    "sec-fetch-site",
  ];

  secFetchHeaders.forEach(header => {
    const expectedValue = gExpectedHeader[header];
    try {
      is(
        channel.getRequestHeader(header),
        expectedValue,
        `${header} is set to ${expectedValue}`
      );
    } catch (e) {
      if (expectedValue) {
        ok(false, "required headers are set");
      } else {
        ok(true, `${header} should not be set`);
      }
    }
  });

  gTestCounter++;
}

async function testNavigations() {
  gTestCounter = 0;

  // Load initial site
  let loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(gBrowser, REQUEST_URL + "?test1");
  await loaded;

  // Load another site
  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.document.notifyUserGestureActivation(); // simulate user activation
    let test2Button = content.document.getElementById("test2_button");
    test2Button.click();
    content.document.clearUserGestureActivation();
  });
  await loaded;
  // Load another site
  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.document.notifyUserGestureActivation(); // simulate user activation
    let test3Button = content.document.getElementById("test3_button");
    test3Button.click();
    content.document.clearUserGestureActivation();
  });
  await loaded;

  gExpectedHeader = {
    "sec-fetch-mode": "navigate",
    "sec-fetch-dest": "document",
    "sec-fetch-site": "same-origin",
    "sec-fetch-user": "?1",
  };

  // Register the http request observer.
  // All following actions should cause requests with the sec-fetch-user header
  // set.
  Services.obs.addObserver(checkSecFetchUser, "http-on-stop-request");

  // Go back one site by clicking the back button
  info("Clicking back button");
  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  document.notifyUserGestureActivation(); // simulate user activation
  let backButton = document.getElementById("back-button");
  backButton.click();
  document.clearUserGestureActivation();
  await loaded;

  // Reload the site by clicking the reload button
  info("Clicking reload button");
  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  document.notifyUserGestureActivation(); // simulate user activation
  let reloadButton = document.getElementById("reload-button");
  await TestUtils.waitForCondition(() => {
    return !reloadButton.disabled;
  });
  reloadButton.click();
  document.clearUserGestureActivation();
  await loaded;

  // Go forward one site by clicking the forward button
  info("Clicking forward button");
  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  document.notifyUserGestureActivation(); // simulate user activation
  let forwardButton = document.getElementById("forward-button");
  forwardButton.click();
  document.clearUserGestureActivation();
  await loaded;

  // Testing history.back/forward...

  info("going back with history.back");
  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.document.notifyUserGestureActivation(); // simulate user activation
    content.history.back();
    content.document.clearUserGestureActivation();
  });
  await loaded;

  info("going forward with history.forward");
  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.document.notifyUserGestureActivation(); // simulate user activation
    content.history.forward();
    content.document.clearUserGestureActivation();
  });
  await loaded;

  gExpectedHeader = {
    "sec-fetch-mode": "navigate",
    "sec-fetch-dest": "document",
    "sec-fetch-site": "same-origin",
  };

  info("going back with history.back without user activation");
  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.history.back();
  });
  await loaded;

  info("going forward with history.forward without user activation");
  loaded = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.history.forward();
  });
  await loaded;

  ok(gTestCounter === 7, "testing that all five actions have been tested.");

  Services.obs.removeObserver(checkSecFetchUser, "http-on-stop-request");
}

add_task(async function () {
  waitForExplicitFinish();

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
