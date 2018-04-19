/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
const URL = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.org") + "trackingPage_iframe.html";

/**
 * Tests that sites that only contain trackers in iframes still
 * get the tracking protection UI.
 */

add_task(async function test_iframe() {
  await UrlClassifierTestUtils.addTestTrackers();

  await SpecialPowers.pushPrefEnv({ set: [[PREF, true]] });

  await BrowserTestUtils.withNewTab(URL, async function(browser) {
    var TrackingProtection = browser.ownerGlobal.TrackingProtection;
    ok(TrackingProtection, "got TP object");
    ok(TrackingProtection.enabled, "TP is enabled");

    is(TrackingProtection.content.getAttribute("state"), "blocked-tracking-content",
        'content: state="blocked-tracking-content"');
    is(TrackingProtection.icon.getAttribute("state"), "blocked-tracking-content",
        'icon: state="blocked-tracking-content"');
    is(TrackingProtection.icon.getAttribute("tooltiptext"),
       gNavigatorBundle.getString("trackingProtection.icon.activeTooltip"), "correct tooltip");
  });

  UrlClassifierTestUtils.cleanupTestTrackers();
});
