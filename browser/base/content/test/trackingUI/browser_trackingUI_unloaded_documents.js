/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
const URL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.org") + "trackingPage_onunload.html";

/**
 * Tests that visiting a new page while a request to a tracker is still
 * in flight (such as when it was triggered on unload) does not cause
 * the tracking UI to appear.
 */

add_task(async function test_onunload() {
  await UrlClassifierTestUtils.addTestTrackers();

  await SpecialPowers.pushPrefEnv({ set: [[PREF, true]] });

  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  let browser = tab.linkedBrowser;

  let loaded = BrowserTestUtils.browserLoaded(browser, false, "http://example.com/");
  await BrowserTestUtils.loadURI(browser, "http://example.com/");
  await loaded;
  var TrackingProtection = browser.ownerGlobal.TrackingProtection;
  ok(TrackingProtection, "got TP object");
  ok(TrackingProtection.enabled, "TP is enabled");

  ok(!TrackingProtection.container.hidden, "The container is visible");
  ok(!TrackingProtection.content.hasAttribute("state"), "content: no state");
  ok(!TrackingProtection.icon.hasAttribute("state"), "icon: no state");
  ok(!TrackingProtection.icon.hasAttribute("tooltiptext"), "icon: no tooltip");

  BrowserTestUtils.removeTab(tab);
  UrlClassifierTestUtils.cleanupTestTrackers();
});
