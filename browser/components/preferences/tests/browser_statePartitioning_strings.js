"use strict";

/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const CB_STRICT_FEATURES_PREF = "browser.contentblocking.features.strict";
const CB_STRICT_FEATURES_VALUE = "tp,tpPrivate,cookieBehavior5,cm,fp,stp,lvl2";
const MVP_UI_PREF = "browser.contentblocking.state-partitioning.mvp.ui.enabled";
const FPI_PREF = "privacy.firstparty.isolate";
const COOKIE_BEHAVIOR_PREF = "network.cookie.cookieBehavior";
const COOKIE_BEHAVIOR_VALUE = 5;

async function testStrings(mvpUIEnabled) {
  info(`Running testStrings with MVP UI pref set to ${MVP_UI_PREF}`);

  SpecialPowers.pushPrefEnv({ set: [[MVP_UI_PREF, mvpUIEnabled]] });
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let doc = gBrowser.contentDocument;

  // Check the cookie blocking info strings
  let elts = doc.querySelectorAll(
    ".extra-information-label.cross-site-cookies-option"
  );
  for (let elt of elts) {
    is(
      elt.hidden,
      !mvpUIEnabled,
      `The new cross-site cookies info label is ${
        mvpUIEnabled ? "visible" : "hidden"
      }`
    );
  }

  elts = doc.querySelectorAll(
    ".extra-information-label.third-party-tracking-cookies-plus-isolate-option"
  );
  for (let elt of elts) {
    is(
      elt.hidden,
      mvpUIEnabled,
      `The old third party cookies info label is ${
        mvpUIEnabled ? "hidden" : "visible"
      }`
    );
  }

  // Check the learn more strings
  elts = doc.querySelectorAll(
    ".tail-with-learn-more.content-blocking-warning-description"
  );
  let expectedStringID = mvpUIEnabled
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
  expectedStringID = mvpUIEnabled
    ? "sitedata-option-block-cross-site-cookies-including-social-media"
    : "sitedata-option-block-cross-site-and-social-media-trackers-plus-isolate";
  is(
    id,
    expectedStringID,
    "The correct string is in use for the cookie blocking option"
  );

  // Check the FPI warning is hidden with FPI off
  let warningElt = doc.getElementById("fpiIncompatibilityWarning");
  is(warningElt.hidden, true, "The FPI warning is hidden");

  gBrowser.removeCurrentTab();

  // Check the FPI warning is shown only if MVP UI is enabled when FPI is on
  await SpecialPowers.pushPrefEnv({ set: [[FPI_PREF, true]] });
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  doc = gBrowser.contentDocument;
  warningElt = doc.getElementById("fpiIncompatibilityWarning");
  is(
    warningElt.hidden,
    !mvpUIEnabled,
    `The FPI warning is ${mvpUIEnabled ? "visible" : "hidden"}`
  );
  await SpecialPowers.popPrefEnv();

  gBrowser.removeCurrentTab();
}

add_task(async function runTests() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [CB_STRICT_FEATURES_PREF, CB_STRICT_FEATURES_VALUE],
      [FPI_PREF, false],
    ],
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
