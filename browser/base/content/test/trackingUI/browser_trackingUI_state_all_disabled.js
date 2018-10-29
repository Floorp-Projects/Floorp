/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";
const BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/benignPage.html";
const TP_PREF = "privacy.trackingprotection.enabled";
const FB_PREF = "browser.fastblock.enabled";
const COOKIE_PREF = "network.cookie.cookieBehavior";
const ANIMATIONS_PREF = "toolkit.cosmeticAnimations.enabled";

// Check that the shield icon is always hidden when all content blocking
// categories are turned off, even when content blocking is on.
add_task(async function testContentBlockingAllDisabled() {
  await SpecialPowers.pushPrefEnv({set: [
    [TP_PREF, false],
    [FB_PREF, false],
    [COOKIE_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT],
  ]});
  await UrlClassifierTestUtils.addTestTrackers();

  let tab = gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  Services.prefs.setBoolPref(ANIMATIONS_PREF, true);

  registerCleanupFunction(function() {
    gBrowser.removeCurrentTab();
    Services.prefs.clearUserPref(ANIMATIONS_PREF);
    UrlClassifierTestUtils.cleanupTestTrackers();
  });

  let animationIcon = document.getElementById("tracking-protection-icon-animatable-image");
  let noAnimationIcon = document.getElementById("tracking-protection-icon");

  await promiseTabLoadEvent(tab, TRACKING_PAGE);
  ok(BrowserTestUtils.is_hidden(noAnimationIcon), "the default icon is hidden");
  ok(BrowserTestUtils.is_hidden(animationIcon), "the animated icon is hidden");

  await promiseTabLoadEvent(tab, BENIGN_PAGE);
  ok(BrowserTestUtils.is_hidden(animationIcon), "the animated icon is hidden");
  ok(BrowserTestUtils.is_hidden(noAnimationIcon), "the default icon is hidden");

  Services.prefs.setBoolPref(ANIMATIONS_PREF, false);
  await promiseTabLoadEvent(tab, TRACKING_PAGE);
  ok(BrowserTestUtils.is_hidden(animationIcon), "the animated icon is hidden");
  ok(BrowserTestUtils.is_hidden(noAnimationIcon), "the default icon is hidden");

  // Sanitity check that the shield is showing when at least one blocker is enabled.
  await SpecialPowers.pushPrefEnv({set: [
    [TP_PREF, true],
  ]});
  await promiseTabLoadEvent(tab, TRACKING_PAGE);
  ok(BrowserTestUtils.is_visible(noAnimationIcon), "the default icon is shown");
});
