/* eslint-env webextensions */

"use strict";

const { Preferences } = ChromeUtils.importESModule(
  "resource://gre/modules/Preferences.sys.mjs"
);

const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PBM_PREF = "privacy.trackingprotection.pbmode.enabled";
const NCB_PREF = "network.cookie.cookieBehavior";
const NCBP_PREF = "network.cookie.cookieBehavior.pbmode";
const CAT_PREF = "browser.contentblocking.category";
const FP_PREF = "privacy.trackingprotection.fingerprinting.enabled";
const STP_PREF = "privacy.trackingprotection.socialtracking.enabled";
const CM_PREF = "privacy.trackingprotection.cryptomining.enabled";
const EMAIL_TP_PREF = "privacy.trackingprotection.emailtracking.enabled";
const EMAIL_TP_PBM_PREF =
  "privacy.trackingprotection.emailtracking.pbmode.enabled";
const LEVEL2_PREF = "privacy.annotate_channels.strict_list.enabled";
const REFERRER_PREF = "network.http.referer.disallowCrossSiteRelaxingDefault";
const REFERRER_TOP_PREF =
  "network.http.referer.disallowCrossSiteRelaxingDefault.top_navigation";
const OCSP_PREF = "privacy.partition.network_state.ocsp_cache";
const QUERY_PARAM_STRIP_PREF = "privacy.query_stripping.enabled";
const QUERY_PARAM_STRIP_PBM_PREF = "privacy.query_stripping.enabled.pbmode";
const PREF_TEST_NOTIFICATIONS =
  "browser.safebrowsing.test-notifications.enabled";
const STRICT_PREF = "browser.contentblocking.features.strict";
const PRIVACY_PAGE = "about:preferences#privacy";
const ISOLATE_UI_PREF =
  "browser.contentblocking.reject-and-isolate-cookies.preferences.ui.enabled";
const FPI_PREF = "privacy.firstparty.isolate";
const FPP_PREF = "privacy.fingerprintingProtection";
const FPP_PBM_PREF = "privacy.fingerprintingProtection.pbmode";

const { EnterprisePolicyTesting, PoliciesPrefTracker } =
  ChromeUtils.importESModule(
    "resource://testing-common/EnterprisePolicyTesting.sys.mjs"
  );

requestLongerTimeout(2);

add_task(async function testListUpdate() {
  SpecialPowers.pushPrefEnv({ set: [[PREF_TEST_NOTIFICATIONS, true]] });

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let doc = gBrowser.contentDocument;

  let fingerprintersCheckbox = doc.getElementById(
    "contentBlockingFingerprintersCheckbox"
  );
  let updateObserved = TestUtils.topicObserved("safebrowsing-update-attempt");
  fingerprintersCheckbox.click();
  let url = (await updateObserved)[1];

  ok(true, "Has tried to update after the fingerprinting checkbox was toggled");
  is(
    url,
    "http://127.0.0.1:8888/safebrowsing-dummy/update",
    "Using the correct list url to update"
  );

  let cryptominersCheckbox = doc.getElementById(
    "contentBlockingCryptominersCheckbox"
  );
  updateObserved = TestUtils.topicObserved("safebrowsing-update-attempt");
  cryptominersCheckbox.click();
  url = (await updateObserved)[1];

  ok(true, "Has tried to update after the cryptomining checkbox was toggled");
  is(
    url,
    "http://127.0.0.1:8888/safebrowsing-dummy/update",
    "Using the correct list url to update"
  );

  gBrowser.removeCurrentTab();
});

// Tests that the content blocking main category checkboxes have the correct default state.
add_task(async function testContentBlockingMainCategory() {
  let prefs = [
    [TP_PREF, false],
    [TP_PBM_PREF, true],
    [STP_PREF, false],
    [NCB_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER],
    [
      NCBP_PREF,
      Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    ],
    [ISOLATE_UI_PREF, true],
    [FPI_PREF, false],
    [FPP_PREF, false],
    [FPP_PBM_PREF, true],
  ];

  for (let pref of prefs) {
    switch (typeof pref[1]) {
      case "boolean":
        SpecialPowers.setBoolPref(pref[0], pref[1]);
        break;
      case "number":
        SpecialPowers.setIntPref(pref[0], pref[1]);
        break;
    }
  }

  let checkboxes = [
    "#contentBlockingTrackingProtectionCheckbox",
    "#contentBlockingBlockCookiesCheckbox",
    "#contentBlockingFingerprintingProtectionCheckbox",
  ];

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let doc = gBrowser.contentDocument;

  for (let selector of checkboxes) {
    let element = doc.querySelector(selector);
    ok(element, "checkbox " + selector + " exists");
    is(
      element.getAttribute("checked"),
      "true",
      "checkbox " + selector + " is checked"
    );
  }

  // Ensure the dependent controls of the tracking protection subsection behave properly.
  let tpCheckbox = doc.querySelector(checkboxes[0]);

  let dependentControls = ["#trackingProtectionMenu"];
  let alwaysEnabledControls = [
    "#trackingProtectionMenuDesc",
    ".content-blocking-category-name",
    "#changeBlockListLink",
  ];

  tpCheckbox.checked = true;

  // Select "Always" under "All Detected Trackers".
  let menu = doc.querySelector("#trackingProtectionMenu");
  let always = doc.querySelector(
    "#trackingProtectionMenu > menupopup > menuitem[value=always]"
  );
  let privateElement = doc.querySelector(
    "#trackingProtectionMenu > menupopup > menuitem[value=private]"
  );
  menu.selectedItem = always;
  ok(
    !privateElement.selected,
    "The Only in private windows item should not be selected"
  );
  ok(always.selected, "The Always item should be selected");

  // The first time, privacy-pane-tp-ui-updated won't be dispatched since the
  // assignment above is a no-op.

  // Ensure the dependent controls are enabled
  checkControlState(doc, dependentControls, true);
  checkControlState(doc, alwaysEnabledControls, true);

  let promise = TestUtils.topicObserved("privacy-pane-tp-ui-updated");
  tpCheckbox.click();

  await promise;
  ok(!tpCheckbox.checked, "The checkbox should now be unchecked");

  // Ensure the dependent controls are disabled
  checkControlState(doc, dependentControls, false);
  checkControlState(doc, alwaysEnabledControls, true);

  // Make sure the selection in the tracking protection submenu persists after
  // a few times of checking and unchecking All Detected Trackers.
  // Doing this in a loop in order to avoid typing in the unrolled version manually.
  // We need to go from the checked state of the checkbox to unchecked back to
  // checked again...
  for (let i = 0; i < 3; ++i) {
    promise = TestUtils.topicObserved("privacy-pane-tp-ui-updated");
    tpCheckbox.click();

    await promise;
    is(tpCheckbox.checked, i % 2 == 0, "The checkbox should now be unchecked");
    is(
      privateElement.selected,
      i % 2 == 0,
      "The Only in private windows item should be selected by default, when the checkbox is checked"
    );
    ok(!always.selected, "The Always item should no longer be selected");
  }

  let cookieMenu = doc.querySelector("#blockCookiesMenu");
  let cookieMenuTrackers = cookieMenu.querySelector(
    "menupopup > menuitem[value=trackers]"
  );
  let cookieMenuTrackersPlusIsolate = cookieMenu.querySelector(
    "menupopup > menuitem[value=trackers-plus-isolate]"
  );
  let cookieMenuUnvisited = cookieMenu.querySelector(
    "menupopup > menuitem[value=unvisited]"
  );
  let cookieMenuAllThirdParties = doc.querySelector(
    "menupopup > menuitem[value=all-third-parties]"
  );
  let cookieMenuAll = cookieMenu.querySelector(
    "menupopup > menuitem[value=always]"
  );
  // Select block trackers
  cookieMenuTrackers.click();
  ok(cookieMenuTrackers.selected, "The trackers item should be selected");
  is(
    Services.prefs.getIntPref(NCB_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
    `${NCB_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER}`
  );
  is(
    Services.prefs.getIntPref(NCBP_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    `${NCBP_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN}`
  );
  // Select block trackers and isolate
  cookieMenuTrackersPlusIsolate.click();
  ok(
    cookieMenuTrackersPlusIsolate.selected,
    "The trackers plus isolate item should be selected"
  );
  is(
    Services.prefs.getIntPref(NCB_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    `${NCB_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN}`
  );
  is(
    Services.prefs.getIntPref(NCBP_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    `${NCBP_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN}`
  );
  // Select block unvisited
  cookieMenuUnvisited.click();
  ok(cookieMenuUnvisited.selected, "The unvisited item should be selected");
  is(
    Services.prefs.getIntPref(NCB_PREF),
    Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN,
    `${NCB_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN}`
  );
  is(
    Services.prefs.getIntPref(NCBP_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    `${NCBP_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN}`
  );
  // Select block all third party
  cookieMenuAllThirdParties.click();
  ok(
    cookieMenuAllThirdParties.selected,
    "The all-third-parties item should be selected"
  );
  is(
    Services.prefs.getIntPref(NCB_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
    `${NCB_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN}`
  );
  is(
    Services.prefs.getIntPref(NCBP_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    `${NCBP_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN}`
  );
  // Select block all third party
  cookieMenuAll.click();
  ok(cookieMenuAll.selected, "The all cookies item should be selected");
  is(
    Services.prefs.getIntPref(NCB_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT,
    `${NCB_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT}`
  );
  is(
    Services.prefs.getIntPref(NCBP_PREF),
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
    `${NCBP_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN}`
  );

  gBrowser.removeCurrentTab();

  // Ensure the block-trackers-plus-isolate option only shows in the dropdown if the UI pref is set.
  Services.prefs.setBoolPref(ISOLATE_UI_PREF, false);
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  doc = gBrowser.contentDocument;
  cookieMenuTrackersPlusIsolate = doc.querySelector(
    "#blockCookiesMenu menupopup > menuitem[value=trackers-plus-isolate]"
  );
  ok(
    cookieMenuTrackersPlusIsolate.hidden,
    "Trackers plus isolate option is hidden from the dropdown if the ui pref is not set."
  );

  gBrowser.removeCurrentTab();

  // Ensure the block-trackers-plus-isolate option only shows in the dropdown if FPI is disabled.
  SpecialPowers.setIntPref(
    NCB_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
  );
  SpecialPowers.setBoolPref(FPI_PREF, true);

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  doc = gBrowser.contentDocument;
  cookieMenuTrackers = doc.querySelector(
    "#blockCookiesMenu menupopup > menuitem[value=trackers]"
  );
  cookieMenuTrackersPlusIsolate = doc.querySelector(
    "#blockCookiesMenu menupopup > menuitem[value=trackers-plus-isolate]"
  );
  ok(cookieMenuTrackers.selected, "The trackers item should be selected");
  ok(
    cookieMenuTrackersPlusIsolate.hidden,
    "Trackers plus isolate option is hidden from the dropdown if the FPI pref is set."
  );
  gBrowser.removeCurrentTab();

  for (let pref of prefs) {
    SpecialPowers.clearUserPref(pref[0]);
  }
});

// Tests that the content blocking "Standard" category radio sets the prefs to their default values.
add_task(async function testContentBlockingStandardCategory() {
  let prefs = {
    [TP_PREF]: null,
    [TP_PBM_PREF]: null,
    [NCB_PREF]: null,
    [NCBP_PREF]: null,
    [FP_PREF]: null,
    [STP_PREF]: null,
    [CM_PREF]: null,
    [EMAIL_TP_PREF]: null,
    [EMAIL_TP_PBM_PREF]: null,
    [LEVEL2_PREF]: null,
    [REFERRER_PREF]: null,
    [REFERRER_TOP_PREF]: null,
    [OCSP_PREF]: null,
    [QUERY_PARAM_STRIP_PREF]: null,
    [QUERY_PARAM_STRIP_PBM_PREF]: null,
    [FPP_PREF]: null,
    [FPP_PBM_PREF]: null,
  };

  for (let pref in prefs) {
    Services.prefs.clearUserPref(pref);
    switch (Services.prefs.getPrefType(pref)) {
      case Services.prefs.PREF_BOOL:
        prefs[pref] = Services.prefs.getBoolPref(pref);
        break;
      case Services.prefs.PREF_INT:
        prefs[pref] = Services.prefs.getIntPref(pref);
        break;
      case Services.prefs.PREF_STRING:
        prefs[pref] = Services.prefs.getCharPref(pref);
        break;
      default:
        ok(false, `Unknown pref type for ${pref}`);
    }
  }

  Services.prefs.setBoolPref(TP_PREF, true);
  Services.prefs.setBoolPref(TP_PBM_PREF, false);
  Services.prefs.setIntPref(
    NCB_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );
  Services.prefs.setIntPref(
    NCBP_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
  );
  Services.prefs.setBoolPref(STP_PREF, !Services.prefs.getBoolPref(STP_PREF));
  Services.prefs.setBoolPref(FP_PREF, !Services.prefs.getBoolPref(FP_PREF));
  Services.prefs.setBoolPref(CM_PREF, !Services.prefs.getBoolPref(CM_PREF));
  Services.prefs.setBoolPref(
    EMAIL_TP_PREF,
    !Services.prefs.getBoolPref(EMAIL_TP_PREF)
  );
  Services.prefs.setBoolPref(
    EMAIL_TP_PBM_PREF,
    !Services.prefs.getBoolPref(EMAIL_TP_PBM_PREF)
  );
  Services.prefs.setBoolPref(
    LEVEL2_PREF,
    !Services.prefs.getBoolPref(LEVEL2_PREF)
  );
  Services.prefs.setBoolPref(
    REFERRER_PREF,
    !Services.prefs.getBoolPref(REFERRER_PREF)
  );
  Services.prefs.setBoolPref(
    REFERRER_TOP_PREF,
    !Services.prefs.getBoolPref(REFERRER_TOP_PREF)
  );
  Services.prefs.setBoolPref(OCSP_PREF, !Services.prefs.getBoolPref(OCSP_PREF));
  Services.prefs.setBoolPref(
    QUERY_PARAM_STRIP_PREF,
    !Services.prefs.getBoolPref(QUERY_PARAM_STRIP_PREF)
  );
  Services.prefs.setBoolPref(
    QUERY_PARAM_STRIP_PBM_PREF,
    !Services.prefs.getBoolPref(QUERY_PARAM_STRIP_PBM_PREF)
  );
  Services.prefs.setBoolPref(FPP_PREF, !Services.prefs.getBoolPref(FPP_PREF));
  Services.prefs.setBoolPref(
    FPP_PBM_PREF,
    !Services.prefs.getBoolPref(FPP_PBM_PREF)
  );

  for (let pref in prefs) {
    switch (Services.prefs.getPrefType(pref)) {
      case Services.prefs.PREF_BOOL:
        // Account for prefs that may have retained their default value
        if (Services.prefs.getBoolPref(pref) != prefs[pref]) {
          ok(
            Services.prefs.prefHasUserValue(pref),
            `modified the pref ${pref}`
          );
        }
        break;
      case Services.prefs.PREF_INT:
        if (Services.prefs.getIntPref(pref) != prefs[pref]) {
          ok(
            Services.prefs.prefHasUserValue(pref),
            `modified the pref ${pref}`
          );
        }
        break;
      case Services.prefs.PREF_STRING:
        if (Services.prefs.getCharPref(pref) != prefs[pref]) {
          ok(
            Services.prefs.prefHasUserValue(pref),
            `modified the pref ${pref}`
          );
        }
        break;
      default:
        ok(false, `Unknown pref type for ${pref}`);
    }
  }

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let doc = gBrowser.contentDocument;

  let standardRadioOption = doc.getElementById("standardRadio");
  standardRadioOption.click();

  // TP prefs are reset async to check for extensions controlling them.
  await TestUtils.waitForCondition(
    () => !Services.prefs.prefHasUserValue(TP_PREF)
  );

  for (let pref in prefs) {
    ok(!Services.prefs.prefHasUserValue(pref), `reset the pref ${pref}`);
  }
  is(
    Services.prefs.getStringPref(CAT_PREF),
    "standard",
    `${CAT_PREF} has been set to standard`
  );

  gBrowser.removeCurrentTab();
});

// Tests that the content blocking "Strict" category radio sets the prefs to the expected values.
add_task(async function testContentBlockingStrictCategory() {
  Services.prefs.setBoolPref(TP_PREF, false);
  Services.prefs.setBoolPref(TP_PBM_PREF, false);
  Services.prefs.setBoolPref(EMAIL_TP_PREF, false);
  Services.prefs.setBoolPref(EMAIL_TP_PBM_PREF, false);
  Services.prefs.setBoolPref(LEVEL2_PREF, false);
  Services.prefs.setBoolPref(REFERRER_PREF, false);
  Services.prefs.setBoolPref(REFERRER_TOP_PREF, false);
  Services.prefs.setBoolPref(OCSP_PREF, false);
  Services.prefs.setBoolPref(QUERY_PARAM_STRIP_PREF, false);
  Services.prefs.setBoolPref(QUERY_PARAM_STRIP_PBM_PREF, false);
  Services.prefs.setBoolPref(FPP_PREF, false);
  Services.prefs.setBoolPref(FPP_PBM_PREF, false);
  Services.prefs.setIntPref(
    NCB_PREF,
    Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN
  );
  Services.prefs.setIntPref(
    NCBP_PREF,
    Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN
  );
  let strict_pref = Services.prefs.getStringPref(STRICT_PREF).split(",");

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let doc = gBrowser.contentDocument;

  let strictRadioOption = doc.getElementById("strictRadio");
  strictRadioOption.click();

  // TP prefs are reset async to check for extensions controlling them.
  await TestUtils.waitForCondition(
    () => Services.prefs.getStringPref(CAT_PREF) == "strict"
  );
  // Depending on the definition of the STRICT_PREF, the dependant prefs may have been
  // set to varying values. Ensure they have been set according to this definition.
  for (let pref of strict_pref) {
    switch (pref) {
      case "tp":
        is(
          Services.prefs.getBoolPref(TP_PREF),
          true,
          `${TP_PREF} has been set to true`
        );
        break;
      case "-tp":
        is(
          Services.prefs.getBoolPref(TP_PREF),
          false,
          `${TP_PREF} has been set to false`
        );
        break;
      case "tpPrivate":
        is(
          Services.prefs.getBoolPref(TP_PBM_PREF),
          true,
          `${TP_PBM_PREF} has been set to true`
        );
        break;
      case "-tpPrivate":
        is(
          Services.prefs.getBoolPref(TP_PBM_PREF),
          false,
          `${TP_PBM_PREF} has been set to false`
        );
        break;
      case "fp":
        is(
          Services.prefs.getBoolPref(FP_PREF),
          true,
          `${FP_PREF} has been set to true`
        );
        break;
      case "-fp":
        is(
          Services.prefs.getBoolPref(FP_PREF),
          false,
          `${FP_PREF} has been set to false`
        );
        break;
      case "stp":
        is(
          Services.prefs.getBoolPref(STP_PREF),
          true,
          `${STP_PREF} has been set to true`
        );
        break;
      case "-stp":
        is(
          Services.prefs.getBoolPref(STP_PREF),
          false,
          `${STP_PREF} has been set to false`
        );
        break;
      case "cm":
        is(
          Services.prefs.getBoolPref(CM_PREF),
          true,
          `${CM_PREF} has been set to true`
        );
        break;
      case "-cm":
        is(
          Services.prefs.getBoolPref(CM_PREF),
          false,
          `${CM_PREF} has been set to false`
        );
        break;
      case "emailTP":
        is(
          Services.prefs.getBoolPref(EMAIL_TP_PREF),
          true,
          `${EMAIL_TP_PREF} has been set to true`
        );
        break;
      case "-emailTP":
        is(
          Services.prefs.getBoolPref(EMAIL_TP_PREF),
          false,
          `${EMAIL_TP_PREF} has been set to false`
        );
        break;
      case "emailTPPrivate":
        is(
          Services.prefs.getBoolPref(EMAIL_TP_PBM_PREF),
          true,
          `${EMAIL_TP_PBM_PREF} has been set to true`
        );
        break;
      case "-emailTPPrivate":
        is(
          Services.prefs.getBoolPref(EMAIL_TP_PBM_PREF),
          false,
          `${EMAIL_TP_PBM_PREF} has been set to false`
        );
        break;
      case "lvl2":
        is(
          Services.prefs.getBoolPref(LEVEL2_PREF),
          true,
          `${CM_PREF} has been set to true`
        );
        break;
      case "-lvl2":
        is(
          Services.prefs.getBoolPref(LEVEL2_PREF),
          false,
          `${CM_PREF} has been set to false`
        );
        break;
      case "rp":
        is(
          Services.prefs.getBoolPref(REFERRER_PREF),
          true,
          `${REFERRER_PREF} has been set to true`
        );
        break;
      case "-rp":
        is(
          Services.prefs.getBoolPref(REFERRER_PREF),
          false,
          `${REFERRER_PREF} has been set to false`
        );
        break;
      case "rpTop":
        is(
          Services.prefs.getBoolPref(REFERRER_TOP_PREF),
          true,
          `${REFERRER_TOP_PREF} has been set to true`
        );
        break;
      case "-rpTop":
        is(
          Services.prefs.getBoolPref(REFERRER_TOP_PREF),
          false,
          `${REFERRER_TOP_PREF} has been set to false`
        );
        break;
      case "ocsp":
        is(
          Services.prefs.getBoolPref(OCSP_PREF),
          true,
          `${OCSP_PREF} has been set to true`
        );
        break;
      case "-ocsp":
        is(
          Services.prefs.getBoolPref(OCSP_PREF),
          false,
          `${OCSP_PREF} has been set to false`
        );
        break;
      case "qps":
        is(
          Services.prefs.getBoolPref(QUERY_PARAM_STRIP_PREF),
          true,
          `${QUERY_PARAM_STRIP_PREF} has been set to true`
        );
        break;
      case "-qps":
        is(
          Services.prefs.getBoolPref(QUERY_PARAM_STRIP_PREF),
          false,
          `${QUERY_PARAM_STRIP_PREF} has been set to false`
        );
        break;
      case "qpsPBM":
        is(
          Services.prefs.getBoolPref(QUERY_PARAM_STRIP_PBM_PREF),
          true,
          `${QUERY_PARAM_STRIP_PBM_PREF} has been set to true`
        );
        break;
      case "-qpsPBM":
        is(
          Services.prefs.getBoolPref(QUERY_PARAM_STRIP_PBM_PREF),
          false,
          `${QUERY_PARAM_STRIP_PBM_PREF} has been set to false`
        );
        break;
      case "fpp":
        is(
          Services.prefs.getBoolPref(FPP_PREF),
          true,
          `${FPP_PREF} has been set to true`
        );
        break;
      case "-fpp":
        is(
          Services.prefs.getBoolPref(FPP_PREF),
          false,
          `${FPP_PREF} has been set to false`
        );
        break;
      case "fppPrivate":
        is(
          Services.prefs.getBoolPref(FPP_PBM_PREF),
          true,
          `${FPP_PBM_PREF} has been set to true`
        );
        break;
      case "-fppPrivate":
        is(
          Services.prefs.getBoolPref(FPP_PBM_PREF),
          false,
          `${FPP_PBM_PREF} has been set to false`
        );
        break;
      case "cookieBehavior0":
        is(
          Services.prefs.getIntPref(NCB_PREF),
          Ci.nsICookieService.BEHAVIOR_ACCEPT,
          `${NCB_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_ACCEPT}`
        );
        break;
      case "cookieBehavior1":
        is(
          Services.prefs.getIntPref(NCB_PREF),
          Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
          `${NCB_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN}`
        );
        break;
      case "cookieBehavior2":
        is(
          Services.prefs.getIntPref(NCB_PREF),
          Ci.nsICookieService.BEHAVIOR_REJECT,
          `${NCB_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT}`
        );
        break;
      case "cookieBehavior3":
        is(
          Services.prefs.getIntPref(NCB_PREF),
          Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN,
          `${NCB_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN}`
        );
        break;
      case "cookieBehavior4":
        is(
          Services.prefs.getIntPref(NCB_PREF),
          Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
          `${NCB_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER}`
        );
        break;
      case "cookieBehavior5":
        is(
          Services.prefs.getIntPref(NCB_PREF),
          Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
          `${NCB_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN}`
        );
        break;
      case "cookieBehaviorPBM0":
        is(
          Services.prefs.getIntPref(NCBP_PREF),
          Ci.nsICookieService.BEHAVIOR_ACCEPT,
          `${NCBP_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_ACCEPT}`
        );
        break;
      case "cookieBehaviorPBM1":
        is(
          Services.prefs.getIntPref(NCBP_PREF),
          Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
          `${NCBP_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN}`
        );
        break;
      case "cookieBehaviorPBM2":
        is(
          Services.prefs.getIntPref(NCBP_PREF),
          Ci.nsICookieService.BEHAVIOR_REJECT,
          `${NCBP_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT}`
        );
        break;
      case "cookieBehaviorPBM3":
        is(
          Services.prefs.getIntPref(NCBP_PREF),
          Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN,
          `${NCBP_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN}`
        );
        break;
      case "cookieBehaviorPBM4":
        is(
          Services.prefs.getIntPref(NCBP_PREF),
          Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
          `${NCBP_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER}`
        );
        break;
      case "cookieBehaviorPBM5":
        is(
          Services.prefs.getIntPref(NCBP_PREF),
          Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
          `${NCBP_PREF} has been set to ${Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN}`
        );
        break;
      default:
        ok(false, "unknown option was added to the strict pref");
        break;
    }
  }

  gBrowser.removeCurrentTab();
});

// Tests that the content blocking "Custom" category behaves as expected.
add_task(async function testContentBlockingCustomCategory() {
  let untouchedPrefs = [
    TP_PREF,
    TP_PBM_PREF,
    NCB_PREF,
    NCBP_PREF,
    FP_PREF,
    STP_PREF,
    CM_PREF,
    REFERRER_PREF,
    REFERRER_TOP_PREF,
    OCSP_PREF,
    QUERY_PARAM_STRIP_PREF,
    QUERY_PARAM_STRIP_PBM_PREF,
  ];

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let doc = gBrowser.contentDocument;
  let strictRadioOption = doc.getElementById("strictRadio");
  let standardRadioOption = doc.getElementById("standardRadio");
  let customRadioOption = doc.getElementById("customRadio");
  let defaults = new Preferences({ defaultBranch: true });

  standardRadioOption.click();
  await TestUtils.waitForCondition(
    () => !Services.prefs.prefHasUserValue(TP_PREF)
  );

  customRadioOption.click();
  await TestUtils.waitForCondition(
    () => Services.prefs.getStringPref(CAT_PREF) == "custom"
  );

  // The custom option will only force change of some prefs, like CAT_PREF. All
  // other prefs should remain as they were for standard.
  for (let pref of untouchedPrefs) {
    ok(
      !Services.prefs.prefHasUserValue(pref),
      `the pref ${pref} remains as default value`
    );
  }

  is(
    Services.prefs.getStringPref(CAT_PREF),
    "custom",
    `${CAT_PREF} has been set to custom`
  );

  strictRadioOption.click();
  await TestUtils.waitForCondition(
    () => Services.prefs.getStringPref(CAT_PREF) == "strict"
  );

  // Changing the following prefs should necessarily set CAT_PREF to "custom"
  for (let pref of [
    FP_PREF,
    STP_PREF,
    CM_PREF,
    TP_PREF,
    TP_PBM_PREF,
    REFERRER_PREF,
    REFERRER_TOP_PREF,
    OCSP_PREF,
    QUERY_PARAM_STRIP_PREF,
    QUERY_PARAM_STRIP_PBM_PREF,
  ]) {
    Services.prefs.setBoolPref(pref, !Services.prefs.getBoolPref(pref));
    await TestUtils.waitForCondition(
      () => Services.prefs.getStringPref(CAT_PREF) == "custom"
    );
    is(
      Services.prefs.getStringPref(CAT_PREF),
      "custom",
      `${CAT_PREF} has been set to custom`
    );

    strictRadioOption.click();
    await TestUtils.waitForCondition(
      () => Services.prefs.getStringPref(CAT_PREF) == "strict"
    );
  }

  // Changing the NCB_PREF should necessarily set CAT_PREF to "custom"
  let defaultNCB = defaults.get(NCB_PREF);
  let nonDefaultNCB;
  switch (defaultNCB) {
    case Ci.nsICookieService.BEHAVIOR_ACCEPT:
      nonDefaultNCB = Ci.nsICookieService.BEHAVIOR_REJECT;
      break;
    case Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER:
    case Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN:
      nonDefaultNCB = Ci.nsICookieService.BEHAVIOR_ACCEPT;
      break;
    default:
      ok(
        false,
        "Unexpected default value found for " + NCB_PREF + ": " + defaultNCB
      );
      break;
  }
  Services.prefs.setIntPref(NCB_PREF, nonDefaultNCB);
  await TestUtils.waitForCondition(() =>
    Services.prefs.prefHasUserValue(NCB_PREF)
  );
  is(
    Services.prefs.getStringPref(CAT_PREF),
    "custom",
    `${CAT_PREF} has been set to custom`
  );

  strictRadioOption.click();
  await TestUtils.waitForCondition(
    () => Services.prefs.getStringPref(CAT_PREF) == "strict"
  );

  // Changing the NCBP_PREF should necessarily set CAT_PREF to "custom"
  let defaultNCBP = defaults.get(NCBP_PREF);
  let nonDefaultNCBP;
  switch (defaultNCBP) {
    case Ci.nsICookieService.BEHAVIOR_ACCEPT:
      nonDefaultNCBP = Ci.nsICookieService.BEHAVIOR_REJECT;
      break;
    case Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER:
    case Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN:
      nonDefaultNCBP = Ci.nsICookieService.BEHAVIOR_ACCEPT;
      break;
    default:
      ok(
        false,
        "Unexpected default value found for " + NCBP_PREF + ": " + defaultNCBP
      );
      break;
  }
  Services.prefs.setIntPref(NCBP_PREF, nonDefaultNCBP);
  await TestUtils.waitForCondition(() =>
    Services.prefs.prefHasUserValue(NCBP_PREF)
  );
  is(
    Services.prefs.getStringPref(CAT_PREF),
    "custom",
    `${CAT_PREF} has been set to custom`
  );

  for (let pref of untouchedPrefs) {
    SpecialPowers.clearUserPref(pref);
  }

  gBrowser.removeCurrentTab();
});

function checkControlState(doc, controls, enabled) {
  for (let selector of controls) {
    for (let control of doc.querySelectorAll(selector)) {
      if (enabled) {
        ok(!control.hasAttribute("disabled"), `${selector} is enabled.`);
      } else {
        is(
          control.getAttribute("disabled"),
          "true",
          `${selector} is disabled.`
        );
      }
    }
  }
}

// Checks that the menulists for tracking protection and cookie blocking are disabled when all TP prefs are off.
add_task(async function testContentBlockingDependentTPControls() {
  SpecialPowers.pushPrefEnv({
    set: [
      [TP_PREF, false],
      [TP_PBM_PREF, false],
      [NCB_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT],
      [CAT_PREF, "custom"],
    ],
  });

  let disabledControls = ["#trackingProtectionMenu", "#blockCookiesMenu"];

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let doc = gBrowser.contentDocument;
  checkControlState(doc, disabledControls, false);

  gBrowser.removeCurrentTab();
});

// Checks that disabling tracking protection also disables email tracking protection.
add_task(async function testDisableTPCheckBoxDisablesEmailTP() {
  SpecialPowers.pushPrefEnv({
    set: [
      [TP_PREF, false],
      [TP_PBM_PREF, true],
      [EMAIL_TP_PREF, false],
      [EMAIL_TP_PBM_PREF, true],
      [CAT_PREF, "custom"],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let doc = gBrowser.contentDocument;

  // Click the checkbox to disable TP and check if this disables Email TP.
  let tpCheckbox = doc.getElementById(
    "contentBlockingTrackingProtectionCheckbox"
  );

  // Verify the initial check state of the tracking protection checkbox.
  is(
    tpCheckbox.getAttribute("checked"),
    "true",
    "Tracking protection checkbox is checked initially"
  );

  tpCheckbox.click();

  // Verify the checkbox is unchecked after clicking.
  is(
    tpCheckbox.getAttribute("checked"),
    "",
    "Tracking protection checkbox is unchecked"
  );

  // Verify the pref states.
  is(
    Services.prefs.getBoolPref(EMAIL_TP_PREF),
    false,
    `${EMAIL_TP_PREF} has been set to false`
  );

  is(
    Services.prefs.getBoolPref(EMAIL_TP_PBM_PREF),
    false,
    `${EMAIL_TP_PBM_PREF} has been set to false`
  );

  gBrowser.removeCurrentTab();
});

// Checks that the email tracking prefs set properly with tracking protection
// drop downs.
add_task(async function testTPMenuForEmailTP() {
  SpecialPowers.pushPrefEnv({
    set: [
      [TP_PREF, false],
      [TP_PBM_PREF, true],
      [EMAIL_TP_PREF, false],
      [EMAIL_TP_PBM_PREF, true],
      [CAT_PREF, "custom"],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let doc = gBrowser.contentDocument;

  let menu = doc.querySelector("#trackingProtectionMenu");
  let always = doc.querySelector(
    "#trackingProtectionMenu > menupopup > menuitem[value=always]"
  );
  let privateElement = doc.querySelector(
    "#trackingProtectionMenu > menupopup > menuitem[value=private]"
  );

  // Click the always option on the tracking protection drop down.
  menu.selectedItem = always;
  always.click();

  // Verify the pref states.
  is(
    Services.prefs.getBoolPref(EMAIL_TP_PREF),
    true,
    `${EMAIL_TP_PREF} has been set to true`
  );

  is(
    Services.prefs.getBoolPref(EMAIL_TP_PBM_PREF),
    true,
    `${EMAIL_TP_PBM_PREF} has been set to true`
  );

  // Click the private-only option on the tracking protection drop down.
  menu.selectedItem = privateElement;
  privateElement.click();

  // Verify the pref states.
  is(
    Services.prefs.getBoolPref(EMAIL_TP_PREF),
    false,
    `${EMAIL_TP_PREF} has been set to false`
  );

  is(
    Services.prefs.getBoolPref(EMAIL_TP_PBM_PREF),
    true,
    `${EMAIL_TP_PBM_PREF} has been set to true`
  );

  gBrowser.removeCurrentTab();
});

// Ensure the FPP checkbox in ETP custom works properly.
add_task(async function testFPPCustomCheckBox() {
  // Set the FPP prefs to the default state.
  SpecialPowers.pushPrefEnv({
    set: [
      [FPP_PREF, false],
      [FPP_PBM_PREF, true],
      [CAT_PREF, "custom"],
    ],
  });

  // Clear glean before testing.
  Services.fog.testResetFOG();

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let doc = gBrowser.contentDocument;

  let fppCheckbox = doc.querySelector(
    "#contentBlockingFingerprintingProtectionCheckbox"
  );

  // Verify the default state of the FPP checkbox.
  ok(fppCheckbox, "FPP checkbox exists");
  is(fppCheckbox.getAttribute("checked"), "true", "FPP checkbox is checked");

  let menu = doc.querySelector("#fingerprintingProtectionMenu");
  let alwaysMenuItem = doc.querySelector(
    "#fingerprintingProtectionMenu > menupopup > menuitem[value=always]"
  );
  let privateMenuItem = doc.querySelector(
    "#fingerprintingProtectionMenu > menupopup > menuitem[value=private]"
  );

  // Click the always option on the FPP drop down.
  menu.selectedItem = alwaysMenuItem;
  alwaysMenuItem.click();

  // Verify the pref states and the telemetry.
  is(
    Services.prefs.getBoolPref(FPP_PREF),
    true,
    `${FPP_PREF} has been set to true`
  );

  is(
    Services.prefs.getBoolPref(FPP_PBM_PREF),
    true,
    `${FPP_PBM_PREF} has been set to true`
  );

  let events = Glean.privacyUiFppClick.menu.testGetValue();
  is(events.length, 1, "The event length is correct");
  is(events[0].extra.value, "always", "The extra field is correct.");

  // Click the private-only option on the FPP drop down.
  menu.selectedItem = privateMenuItem;
  privateMenuItem.click();

  // Verify the pref states and the telemetry.
  is(
    Services.prefs.getBoolPref(FPP_PREF),
    false,
    `${FPP_PREF} has been set to true`
  );

  is(
    Services.prefs.getBoolPref(FPP_PBM_PREF),
    true,
    `${FPP_PBM_PREF} has been set to true`
  );

  events = Glean.privacyUiFppClick.menu.testGetValue();
  is(events.length, 2, "The event length is correct");
  is(events[1].extra.value, "private", "The extra field is correct.");

  // Uncheck the checkbox
  fppCheckbox.click();

  // Verify the pref states and the telemetry.
  is(
    Services.prefs.getBoolPref(FPP_PREF),
    false,
    `${FPP_PREF} has been set to true`
  );

  is(
    Services.prefs.getBoolPref(FPP_PBM_PREF),
    false,
    `${FPP_PBM_PREF} has been set to true`
  );
  is(menu.disabled, true, "The menu is disabled as the checkbox is unchecked");

  events = Glean.privacyUiFppClick.checkbox.testGetValue();
  is(events.length, 1, "The event length is correct");
  is(events[0].extra.checked, "false", "The extra field is correct.");

  // Check the checkbox again.
  fppCheckbox.click();

  // Verify the pref states and telemetry.
  is(
    Services.prefs.getBoolPref(FPP_PREF),
    false,
    `${FPP_PREF} has been set to true`
  );

  is(
    Services.prefs.getBoolPref(FPP_PBM_PREF),
    true,
    `${FPP_PBM_PREF} has been set to true`
  );
  is(menu.disabled, false, "The menu is enabled as the checkbox is checked");

  events = Glean.privacyUiFppClick.checkbox.testGetValue();
  is(events.length, 2, "The event length is correct");
  is(events[1].extra.checked, "true", "The extra field is correct.");

  gBrowser.removeCurrentTab();
});

// Checks that social media trackers, cryptomining and fingerprinting visibility
// can be controlled via pref.
add_task(async function testCustomOptionsVisibility() {
  Services.prefs.setBoolPref(
    "browser.contentblocking.cryptomining.preferences.ui.enabled",
    false
  );
  Services.prefs.setBoolPref(
    "browser.contentblocking.fingerprinting.preferences.ui.enabled",
    false
  );
  Services.prefs.setBoolPref(
    "privacy.socialtracking.block_cookies.enabled",
    false
  );

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  let doc = gBrowser.contentDocument;
  let cryptominersOption = doc.getElementById(
    "contentBlockingCryptominersOption"
  );
  let fingerprintersOption = doc.getElementById(
    "contentBlockingFingerprintersOption"
  );

  ok(cryptominersOption.hidden, "Cryptomining is hidden");
  ok(fingerprintersOption.hidden, "Fingerprinting is hidden");

  gBrowser.removeCurrentTab();

  Services.prefs.setBoolPref(
    "browser.contentblocking.cryptomining.preferences.ui.enabled",
    true
  );

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  doc = gBrowser.contentDocument;
  cryptominersOption = doc.getElementById("contentBlockingCryptominersOption");
  fingerprintersOption = doc.getElementById(
    "contentBlockingFingerprintersOption"
  );

  ok(!cryptominersOption.hidden, "Cryptomining is shown");
  ok(fingerprintersOption.hidden, "Fingerprinting is hidden");

  gBrowser.removeCurrentTab();

  Services.prefs.setBoolPref(
    "browser.contentblocking.fingerprinting.preferences.ui.enabled",
    true
  );

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  doc = gBrowser.contentDocument;
  cryptominersOption = doc.getElementById("contentBlockingCryptominersOption");
  fingerprintersOption = doc.getElementById(
    "contentBlockingFingerprintersOption"
  );

  ok(!cryptominersOption.hidden, "Cryptomining is shown");
  ok(!fingerprintersOption.hidden, "Fingerprinting is shown");

  gBrowser.removeCurrentTab();

  // Social media trackers UI should be hidden
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  doc = gBrowser.contentDocument;
  let socialTrackingUI = [...doc.querySelectorAll(".social-media-option")];

  ok(
    socialTrackingUI.every(el => el.hidden),
    "All Social media tracker UI instances are hidden"
  );

  gBrowser.removeCurrentTab();

  // Social media trackers UI should be visible
  Services.prefs.setBoolPref(
    "privacy.socialtracking.block_cookies.enabled",
    true
  );

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });

  doc = gBrowser.contentDocument;
  socialTrackingUI = [...doc.querySelectorAll(".social-media-option")];

  ok(
    !socialTrackingUI.every(el => el.hidden),
    "All Social media tracker UI instances are visible"
  );

  gBrowser.removeCurrentTab();

  Services.prefs.clearUserPref(
    "browser.contentblocking.cryptomining.preferences.ui.enabled"
  );
  Services.prefs.clearUserPref(
    "browser.contentblocking.fingerprinting.preferences.ui.enabled"
  );
  Services.prefs.clearUserPref("privacy.socialtracking.block_cookies.enabled");
});

// Checks that adding a custom enterprise policy will put the user in the custom category.
// Other categories will be disabled.
add_task(async function testPolicyCategorization() {
  Services.prefs.setStringPref(CAT_PREF, "standard");
  is(
    Services.prefs.getStringPref(CAT_PREF),
    "standard",
    `${CAT_PREF} starts on standard`
  );
  ok(
    !Services.prefs.prefHasUserValue(TP_PREF),
    `${TP_PREF} starts with the default value`
  );
  PoliciesPrefTracker.start();

  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      EnableTrackingProtection: {
        Value: true,
      },
    },
  });
  EnterprisePolicyTesting.checkPolicyPref(TP_PREF, true, false);
  is(
    Services.prefs.getStringPref(CAT_PREF),
    "custom",
    `${CAT_PREF} has been set to custom`
  );

  Services.prefs.setStringPref(CAT_PREF, "standard");
  is(
    Services.prefs.getStringPref(CAT_PREF),
    "standard",
    `${CAT_PREF} starts on standard`
  );
  ok(
    !Services.prefs.prefHasUserValue(NCB_PREF),
    `${NCB_PREF} starts with the default value`
  );
  ok(
    !Services.prefs.prefHasUserValue(NCBP_PREF),
    `${NCBP_PREF} starts with the default value`
  );

  let uiUpdatedPromise = TestUtils.topicObserved("privacy-pane-tp-ui-updated");
  await EnterprisePolicyTesting.setupPolicyEngineWithJson({
    policies: {
      Cookies: {
        AcceptThirdParty: "never",
        Locked: true,
      },
    },
  });
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  await uiUpdatedPromise;

  EnterprisePolicyTesting.checkPolicyPref(
    NCB_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
    true
  );
  EnterprisePolicyTesting.checkPolicyPref(
    NCBP_PREF,
    Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN,
    true
  );
  is(
    Services.prefs.getStringPref(CAT_PREF),
    "custom",
    `${CAT_PREF} has been set to custom`
  );

  let doc = gBrowser.contentDocument;
  let strictRadioOption = doc.getElementById("strictRadio");
  let standardRadioOption = doc.getElementById("standardRadio");
  is(strictRadioOption.disabled, true, "the strict option is disabled");
  is(standardRadioOption.disabled, true, "the standard option is disabled");

  gBrowser.removeCurrentTab();

  // Cleanup after this particular test.
  if (Services.policies.status != Ci.nsIEnterprisePolicies.INACTIVE) {
    await EnterprisePolicyTesting.setupPolicyEngineWithJson({
      policies: {
        Cookies: {
          Locked: false,
        },
      },
    });
    await EnterprisePolicyTesting.setupPolicyEngineWithJson("");
  }
  is(
    Services.policies.status,
    Ci.nsIEnterprisePolicies.INACTIVE,
    "Engine is inactive at the end of the test"
  );

  EnterprisePolicyTesting.resetRunOnceState();
  PoliciesPrefTracker.stop();
});

// Tests that changing a content blocking pref shows the content blocking warning
// to reload tabs to apply changes.
add_task(async function testContentBlockingReloadWarning() {
  Services.prefs.setStringPref(CAT_PREF, "standard");
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let doc = gBrowser.contentDocument;
  let reloadWarnings = [
    ...doc.querySelectorAll(".content-blocking-warning.reload-tabs"),
  ];
  let allHidden = reloadWarnings.every(el => el.hidden);
  ok(allHidden, "all of the warnings to reload tabs are initially hidden");

  Services.prefs.setStringPref(CAT_PREF, "strict");

  let strictWarning = doc.querySelector(
    "#contentBlockingOptionStrict .content-blocking-warning.reload-tabs"
  );
  ok(
    !BrowserTestUtils.is_hidden(strictWarning),
    "The warning in the strict section should be showing"
  );

  Services.prefs.setStringPref(CAT_PREF, "standard");
  gBrowser.removeCurrentTab();
});

// Tests that changing a content blocking pref does not show the content blocking warning
// if it is the only tab.
add_task(async function testContentBlockingReloadWarningSingleTab() {
  Services.prefs.setStringPref(CAT_PREF, "standard");
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    PRIVACY_PAGE
  );
  await BrowserTestUtils.browserLoaded(
    gBrowser.selectedBrowser,
    false,
    PRIVACY_PAGE
  );

  let reloadWarnings = [
    ...gBrowser.contentDocument.querySelectorAll(
      ".content-blocking-warning.reload-tabs"
    ),
  ];
  ok(reloadWarnings.length, "must have at least one reload warning");
  ok(
    reloadWarnings.every(el => el.hidden),
    "all of the warnings to reload tabs are initially hidden"
  );

  is(BrowserWindowTracker.windowCount, 1, "There is only one window open");
  is(gBrowser.tabs.length, 1, "There is only one tab open");
  Services.prefs.setStringPref(CAT_PREF, "strict");

  ok(
    reloadWarnings.every(el => el.hidden),
    "all of the warnings to reload tabs are still hidden"
  );
  Services.prefs.setStringPref(CAT_PREF, "standard");
  BrowserTestUtils.startLoadingURIString(
    gBrowser.selectedBrowser,
    "about:newtab"
  );
  await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
});

// Checks that the reload tabs message reloads all tabs except the active tab.
add_task(async function testReloadTabsMessage() {
  Services.prefs.setStringPref(CAT_PREF, "strict");
  let exampleTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com"
  );
  let examplePinnedTab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com"
  );
  gBrowser.pinTab(examplePinnedTab);
  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let doc = gBrowser.contentDocument;
  let standardWarning = doc.querySelector(
    "#contentBlockingOptionStandard .content-blocking-warning.reload-tabs"
  );
  let standardReloadButton = doc.querySelector(
    "#contentBlockingOptionStandard .reload-tabs-button"
  );

  Services.prefs.setStringPref(CAT_PREF, "standard");
  ok(
    !BrowserTestUtils.is_hidden(standardWarning),
    "The warning in the standard section should be showing"
  );

  let exampleTabBrowserDiscardedPromise = BrowserTestUtils.waitForEvent(
    exampleTab,
    "TabBrowserDiscarded"
  );
  let examplePinnedTabLoadPromise = BrowserTestUtils.browserLoaded(
    examplePinnedTab.linkedBrowser
  );
  standardReloadButton.click();
  // The pinned example page had a load event
  await examplePinnedTabLoadPromise;
  // The other one had its browser discarded
  await exampleTabBrowserDiscardedPromise;

  ok(
    BrowserTestUtils.is_hidden(standardWarning),
    "The warning in the standard section should have hidden after being clicked"
  );

  // cleanup
  Services.prefs.setStringPref(CAT_PREF, "standard");
  gBrowser.removeTab(exampleTab);
  gBrowser.removeTab(examplePinnedTab);
  gBrowser.removeCurrentTab();
});

// Checks that the RFP warning banner is properly shown when rfp prefs are enabled.
add_task(async function testRFPWarningBanner() {
  // Set the prefs to false before testing.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", false],
      ["privacy.resistFingerprinting.pbmode", false],
    ],
  });

  await openPreferencesViaOpenPreferencesAPI("privacy", { leaveOpen: true });
  let doc = gBrowser.contentDocument;
  let rfpWarningBanner = doc.getElementById("rfpIncompatibilityWarning");

  // Verify if the banner is hidden at the beginning.
  ok(
    !BrowserTestUtils.is_visible(rfpWarningBanner),
    "The RFP warning banner is hidden at the beginning."
  );

  // Enable the RFP pref
  await SpecialPowers.pushPrefEnv({
    set: [["privacy.resistFingerprinting", true]],
  });

  // Verify if the banner is shown.
  ok(
    BrowserTestUtils.is_visible(rfpWarningBanner),
    "The RFP warning banner is shown."
  );

  // Enable the RFP pref for private windows
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", false],
      ["privacy.resistFingerprinting.pbmode", true],
    ],
  });

  // Verify if the banner is shown.
  ok(
    BrowserTestUtils.is_visible(rfpWarningBanner),
    "The RFP warning banner is shown."
  );

  // Enable both RFP prefs.
  await SpecialPowers.pushPrefEnv({
    set: [
      ["privacy.resistFingerprinting", true],
      ["privacy.resistFingerprinting.pbmode", true],
    ],
  });

  // Verify if the banner is shown.
  ok(
    BrowserTestUtils.is_visible(rfpWarningBanner),
    "The RFP warning banner is shown."
  );

  gBrowser.removeCurrentTab();
});
