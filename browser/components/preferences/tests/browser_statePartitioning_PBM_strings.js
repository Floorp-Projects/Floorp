"use strict";

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const COOKIE_BEHAVIOR_PREF = "network.cookie.cookieBehavior";
const COOKIE_BEHAVIOR_PBM_PREF = "network.cookie.cookieBehavior.pbmode";

const CB_STRICT_FEATURES_PREF = "browser.contentblocking.features.strict";
const FPI_PREF = "privacy.firstparty.isolate";

async function testCookieBlockingInfoStandard(
  cookieBehavior,
  cookieBehaviorPBM,
  isShown
) {
  let defaults = Services.prefs.getDefaultBranch("");
  defaults.setIntPref(COOKIE_BEHAVIOR_PREF, cookieBehavior);
  defaults.setIntPref(COOKIE_BEHAVIOR_PBM_PREF, cookieBehaviorPBM);

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let doc = gBrowser.contentDocument;

  // Select standard mode.
  let standardRadioOption = doc.getElementById("standardRadio");
  standardRadioOption.click();

  // Check the cookie blocking info for private windows for standard mode.
  let elts = doc.querySelectorAll(
    "#contentBlockingOptionStandard .extra-information-label.all-third-party-cookies-private-windows-option"
  );
  for (let elt of elts) {
    is(
      elt.hidden,
      !isShown,
      `The visibility of cookie blocking info for standard mode is correct`
    );
  }

  gBrowser.removeCurrentTab();
}

async function testCookieBlockingInfoStrict(
  contentBlockingStrictFeatures,
  isShown
) {
  await SpecialPowers.pushPrefEnv({
    set: [
      [CB_STRICT_FEATURES_PREF, contentBlockingStrictFeatures],
      [FPI_PREF, false],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let doc = gBrowser.contentDocument;

  // Select strict mode.
  let strictRadioOption = doc.getElementById("strictRadio");
  strictRadioOption.click();

  // Check the cookie blocking info for private windows for strict mode.
  let elts = doc.querySelectorAll(
    "#contentBlockingOptionStrict .extra-information-label.all-third-party-cookies-private-windows-option"
  );
  for (let elt of elts) {
    is(
      elt.hidden,
      !isShown,
      `The cookie blocking info is hidden for strict mode`
    );
  }

  gBrowser.removeCurrentTab();
}

add_task(async function runTests() {
  await SpecialPowers.pushPrefEnv({
    set: [[FPI_PREF, false]],
  });

  let defaults = Services.prefs.getDefaultBranch("");
  let originalCookieBehavior = defaults.getIntPref(COOKIE_BEHAVIOR_PREF);
  let originalCookieBehaviorPBM = defaults.getIntPref(COOKIE_BEHAVIOR_PBM_PREF);

  // Test if the cookie blocking info for state partitioning in PBM is
  // shown in standard mode if the regular cookieBehavior is
  // BEHAVIOR_REJECT_TRACKER and the private cookieBehavior is
  // BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
  await testCookieBlockingInfoStandard(
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    true
  );

  // Test if the cookie blocking info is hidden in standard mode if both
  // cookieBehaviors are the same.
  await testCookieBlockingInfoStandard(
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    false
  );

  // Test if the cookie blocking info is hidden for strict mode if
  // cookieBehaviors both are BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN in
  // the strict feature value.
  await testCookieBlockingInfoStrict(
    "tp,tpPrivate,cookieBehavior5,cookieBehaviorPBM5,cm,fp,stp,emailTP,emailTPPrivate,lvl2,rp,rpTop,ocsp,fpp,fppPrivate",
    false
  );

  // Test if the cookie blocking info is shown for strict mode if the regular
  // cookieBehavior is BEHAVIOR_REJECT_TRACKER and the private cookieBehavior is
  // BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
  await testCookieBlockingInfoStrict(
    "tp,tpPrivate,cookieBehavior4,cookieBehaviorPBM5,cm,fp,stp,emailTP,emailTPPrivate,lvl2,rp,rpTop,ocsp,fpp,fppPrivate",
    true
  );

  defaults.setIntPref(COOKIE_BEHAVIOR_PREF, originalCookieBehavior);
  defaults.setIntPref(COOKIE_BEHAVIOR_PBM_PREF, originalCookieBehaviorPBM);
  await SpecialPowers.popPrefEnv();
});
