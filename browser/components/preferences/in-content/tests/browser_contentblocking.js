/* eslint-env webextensions */

ChromeUtils.defineModuleGetter(
  this,
  "Preferences",
  "resource://gre/modules/Preferences.jsm"
);

const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PBM_PREF = "privacy.trackingprotection.pbmode.enabled";
const NCB_PREF = "network.cookie.cookieBehavior";
const CAT_PREF = "browser.contentblocking.category";
const FP_PREF = "privacy.trackingprotection.fingerprinting.enabled";
const STP_PREF = "privacy.trackingprotection.socialtracking.enabled";
const CM_PREF = "privacy.trackingprotection.cryptomining.enabled";
const PREF_TEST_NOTIFICATIONS =
  "browser.safebrowsing.test-notifications.enabled";
const STRICT_PREF = "browser.contentblocking.features.strict";
const PRIVACY_PAGE = "about:preferences#privacy";

const { EnterprisePolicyTesting, PoliciesPrefTracker } = ChromeUtils.import(
  "resource://testing-common/EnterprisePolicyTesting.jsm",
  null
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
    [NCB_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER],
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
  let private = doc.querySelector(
    "#trackingProtectionMenu > menupopup > menuitem[value=private]"
  );
  menu.selectedItem = always;
  ok(
    !private.selected,
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
      private.selected,
      i % 2 == 0,
      "The Only in private windows item should be selected by default, when the checkbox is checked"
    );
    ok(!always.selected, "The Always item should no longer be selected");
  }

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
    [FP_PREF]: null,
    [STP_PREF]: null,
    [CM_PREF]: null,
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
  Services.prefs.setBoolPref(STP_PREF, !Services.prefs.getBoolPref(STP_PREF));
  Services.prefs.setBoolPref(FP_PREF, !Services.prefs.getBoolPref(FP_PREF));
  Services.prefs.setBoolPref(CM_PREF, !Services.prefs.getBoolPref(CM_PREF));

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
  Services.prefs.setIntPref(
    NCB_PREF,
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
          `${NCB_PREF} has been set to ${
            Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN
          }`
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
          `${NCB_PREF} has been set to ${
            Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN
          }`
        );
        break;
      case "cookieBehavior4":
        is(
          Services.prefs.getIntPref(NCB_PREF),
          Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER,
          `${NCB_PREF} has been set to ${
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER
          }`
        );
        break;
      case "cookieBehavior5":
        is(
          Services.prefs.getIntPref(NCB_PREF),
          Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN,
          `${NCB_PREF} has been set to ${
            Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN
          }`
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
  let prefs = [TP_PREF, TP_PBM_PREF, NCB_PREF, FP_PREF, STP_PREF, CM_PREF];

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
  // The custom option does not force changes of any prefs, other than CAT_PREF, all other TP prefs should remain as they were for standard.
  for (let pref of prefs) {
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

  // Changing the FP_PREF, STP_PREF, CM_PREF, TP_PREF, or TP_PBM_PREF should necessarily set CAT_PREF to "custom"
  for (let pref of [FP_PREF, STP_PREF, CM_PREF, TP_PREF, TP_PBM_PREF]) {
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

  for (let pref of prefs) {
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

// Checks that cryptomining and fingerprinting visibility can be controlled via pref.
add_task(async function testCustomOptionsVisibility() {
  Services.prefs.setBoolPref(
    "browser.contentblocking.cryptomining.preferences.ui.enabled",
    false
  );
  Services.prefs.setBoolPref(
    "browser.contentblocking.fingerprinting.preferences.ui.enabled",
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

  Services.prefs.clearUserPref(
    "browser.contentblocking.cryptomining.preferences.ui.enabled"
  );
  Services.prefs.clearUserPref(
    "browser.contentblocking.fingerprinting.preferences.ui.enabled"
  );
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
add_task(async function testContentBlockingReloadWarning() {
  Services.prefs.setStringPref(CAT_PREF, "standard");
  await BrowserTestUtils.loadURI(gBrowser.selectedBrowser, PRIVACY_PAGE);

  let reloadWarnings = [
    ...gBrowser.contentDocument.querySelectorAll(
      ".content-blocking-warning.reload-tabs"
    ),
  ];
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
  await BrowserTestUtils.loadURI(gBrowser.selectedBrowser, "about:newtab");
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
