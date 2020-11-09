"use strict";

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const CB_STRICT_FEATURES_PREF = "browser.contentblocking.features.strict";
const CB_STRICT_FEATURES_VALUE = "tp,tpPrivate,cookieBehavior5,cm,fp,stp";
const MVP_UI_PREF = "browser.contentblocking.state-partitioning.mvp.ui.enabled";
const COOKIE_BEHAVIOR_PREF = "network.cookie.cookieBehavior";
const COOKIE_BEHAVIOR_VALUE = 5;

async function testStrings(mvpUIEnabed) {
  await SpecialPowers.pushPrefEnv({ set: [[MVP_UI_PREF, mvpUIEnabed]] });
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let doc = gBrowser.contentDocument;

  // Check the cookie blocking info strings
  let elts = doc.querySelectorAll(
    ".extra-information-label.cross-site-cookies-option"
  );
  for (let elt of elts) {
    is(elt.hidden, !mvpUIEnabed, "The correct element is visible");
  }

  elts = doc.querySelectorAll(
    ".extra-information-label.third-party-tracking-cookies-plus-isolate-option"
  );
  for (let elt of elts) {
    is(elt.hidden, mvpUIEnabed, `The old element ${elt.id} is hidden`);
  }

  // Check the learn more strings
  elts = doc.querySelectorAll(
    ".tail-with-learn-more.content-blocking-warning-description"
  );
  let expectedStringID = mvpUIEnabed
    ? "content-blocking-and-isolating-etp-warning-description-2"
    : "content-blocking-and-isolating-etp-warning-description";
  for (let elt of elts) {
    let id = doc.l10n.getAttributes(elt).id;
    is(
      id,
      expectedStringID,
      "The correct warning description string is in use"
    );
  }

  // Check the cookie blocking mode menu option string
  let elt = doc.querySelector("#isolateCookiesSocialMedia");
  let id = doc.l10n.getAttributes(elt).id;
  expectedStringID = mvpUIEnabed
    ? "sitedata-option-block-cross-site-cookies-including-social-media"
    : "sitedata-option-block-cross-site-and-social-media-trackers-plus-isolate";
  is(
    id,
    expectedStringID,
    "The correct string is in use for the cookie blocking option"
  );

  gBrowser.removeCurrentTab();
}

add_task(async function runTests() {
  await SpecialPowers.pushPrefEnv({
    set: [[CB_STRICT_FEATURES_PREF, CB_STRICT_FEATURES_VALUE]],
  });
  let defaults = Services.prefs.getDefaultBranch("");
  let originalCookieBehavior = defaults.getIntPref(COOKIE_BEHAVIOR_PREF);
  defaults.setIntPref(COOKIE_BEHAVIOR_PREF, COOKIE_BEHAVIOR_VALUE);
  registerCleanupFunction(() => {
    defaults.setIntPref(COOKIE_BEHAVIOR_PREF, originalCookieBehavior);
  });

  await testStrings(true);
  await testStrings(false);
});
