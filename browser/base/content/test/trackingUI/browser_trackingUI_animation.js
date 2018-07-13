/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TRACKING_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/trackingPage.html";
const BENIGN_PAGE = "http://tracking.example.org/browser/browser/base/content/test/trackingUI/benignPage.html";
const TP_PREF = "privacy.trackingprotection.enabled";
const ANIMATIONS_PREF = "toolkit.cosmeticAnimations.enabled";

// Test that the shield icon animation can be controlled by the cosmetic
// animations pref and that one of the icons is visible in each case.
add_task(async function testShieldAnimation() {
  await UrlClassifierTestUtils.addTestTrackers();
  Services.prefs.setBoolPref(TP_PREF, true);

  let tab = gBrowser.selectedTab = gBrowser.addTab();

  let animationIcon = document.getElementById("tracking-protection-icon-animatable-image");
  let noAnimationIcon = document.getElementById("tracking-protection-icon");

  Services.prefs.setBoolPref(ANIMATIONS_PREF, true);
  await promiseTabLoadEvent(tab, TRACKING_PAGE);
  ok(BrowserTestUtils.is_hidden(noAnimationIcon), "the default icon is hidden when animations are enabled");
  ok(BrowserTestUtils.is_visible(animationIcon), "the animated icon is shown when animations are enabled");

  await promiseTabLoadEvent(tab, BENIGN_PAGE);
  ok(BrowserTestUtils.is_hidden(animationIcon), "the animated icon is hidden");
  ok(BrowserTestUtils.is_hidden(noAnimationIcon), "the default icon is hidden");

  Services.prefs.setBoolPref(ANIMATIONS_PREF, false);
  await promiseTabLoadEvent(tab, TRACKING_PAGE);
  ok(BrowserTestUtils.is_visible(noAnimationIcon), "the default icon is shown when animations are disabled");
  ok(BrowserTestUtils.is_hidden(animationIcon), "the animated icon is hidden when animations are disabled");

  gBrowser.removeCurrentTab();
  Services.prefs.clearUserPref(ANIMATIONS_PREF);
  Services.prefs.clearUserPref(TP_PREF);
  UrlClassifierTestUtils.cleanupTestTrackers();
});
