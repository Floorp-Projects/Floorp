/* eslint-env webextensions */

const CB_TP_UI_PREF = "browser.contentblocking.trackingprotection.ui.enabled";
const CB_RT_UI_PREF = "browser.contentblocking.rejecttrackers.ui.enabled";
const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PBM_PREF = "privacy.trackingprotection.pbmode.enabled";
const TP_LIST_PREF = "urlclassifier.trackingTable";
const NCB_PREF = "network.cookie.cookieBehavior";

requestLongerTimeout(2);

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

  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let doc = gBrowser.contentDocument;

  for (let selector of checkboxes) {
    let element = doc.querySelector(selector);
    ok(element, "checkbox " + selector + " exists");
    is(element.getAttribute("checked"), "true",
       "checkbox " + selector + " is checked");
  }

  // Ensure the dependent controls of the tracking protection subsection behave properly.
  let tpCheckbox = doc.querySelector(checkboxes[0]);

  let dependentControls = [
    "#trackingProtectionMenu",
  ];
  let alwaysEnabledControls = [
    "#trackingProtectionMenuDesc",
    ".content-blocking-category-name",
    "#changeBlockListLink",
  ];

  tpCheckbox.checked = true;

  // Select "Always" under "All Detected Trackers".
  let always = doc.querySelector("#trackingProtectionMenu > radio[value=always]");
  let private = doc.querySelector("#trackingProtectionMenu > radio[value=private]");
  always.radioGroup.selectedItem = always;
  ok(!private.selected, "The Only in private windows item should not be selected");
  ok(always.selected, "The Always item should be selected");

  // The first time, privacy-pane-tp-ui-updated won't be dispatched since the
  // assignment above is a no-op.

  // Ensure the dependent controls are enabled
  checkControlState(doc, dependentControls, true);
  checkControlState(doc, alwaysEnabledControls, true);

  let promise = TestUtils.topicObserved("privacy-pane-tp-ui-updated");
  EventUtils.synthesizeMouseAtCenter(tpCheckbox, {}, doc.defaultView);

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
    EventUtils.synthesizeMouseAtCenter(tpCheckbox, {}, doc.defaultView);

    await promise;
    is(tpCheckbox.checked, i % 2 == 0, "The checkbox should now be unchecked");
    ok(!private.selected, "The Only in private windows item should still not be selected");
    ok(always.selected, "The Always item should still be selected");
  }

  gBrowser.removeCurrentTab();

  for (let pref of prefs) {
    SpecialPowers.clearUserPref(pref[0]);
  }
});

// Tests that the content blocking "Restore Defaults" button does what it's supposed to.
add_task(async function testContentBlockingRestoreDefaults() {
  let prefs = {
    [TP_LIST_PREF]: null,
    [TP_PREF]: null,
    [TP_PBM_PREF]: null,
    [NCB_PREF]: null,
  };

  for (let pref in prefs) {
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

  Services.prefs.setStringPref(TP_LIST_PREF, "test-track-simple,base-track-digest256,content-track-digest256");
  Services.prefs.setBoolPref(TP_PREF, true);
  Services.prefs.setBoolPref(TP_PBM_PREF, false);
  Services.prefs.setIntPref(NCB_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);

  for (let pref in prefs) {
    switch (Services.prefs.getPrefType(pref)) {
    case Services.prefs.PREF_BOOL:
      // Account for prefs that may have retained their default value
      if (Services.prefs.getBoolPref(pref) != prefs[pref]) {
        ok(Services.prefs.prefHasUserValue(pref), `modified the pref ${pref}`);
      }
      break;
    case Services.prefs.PREF_INT:
      if (Services.prefs.getIntPref(pref) != prefs[pref]) {
        ok(Services.prefs.prefHasUserValue(pref), `modified the pref ${pref}`);
      }
      break;
    case Services.prefs.PREF_STRING:
      if (Services.prefs.getCharPref(pref) != prefs[pref]) {
        ok(Services.prefs.prefHasUserValue(pref), `modified the pref ${pref}`);
      }
      break;
    default:
      ok(false, `Unknown pref type for ${pref}`);
    }
  }

  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let doc = gBrowser.contentDocument;

  let contentBlockingRestoreDefaults = doc.getElementById("contentBlockingRestoreDefaults");
  contentBlockingRestoreDefaults.click();

  // TP prefs are reset async to check for extensions controlling them.
  await TestUtils.waitForCondition(() => !Services.prefs.prefHasUserValue(TP_PREF));

  for (let pref in prefs) {
    ok(!Services.prefs.prefHasUserValue(pref), `reset the pref ${pref}`);
  }

  gBrowser.removeCurrentTab();
});

// Tests that the content blocking "Restore Defaults" button does not restore prefs
// that are controlled by extensions.
add_task(async function testContentBlockingRestoreDefaultsSkipExtensionControlled() {
  function background() {
    browser.privacy.websites.trackingProtectionMode.set({value: "always"});
  }

  // Install an extension that sets Tracking Protection.
  let extension = ExtensionTestUtils.loadExtension({
    useAddonManager: "permanent",
    manifest: {
      name: "set_tp",
      applications: {gecko: {id: "@set_tp"}},
      permissions: ["privacy"],
    },
    background,
  });

  let resettable = {
    [TP_LIST_PREF]: null,
    [NCB_PREF]: null,
  };

  for (let pref in resettable) {
    switch (Services.prefs.getPrefType(pref)) {
    case Services.prefs.PREF_STRING:
      resettable[pref] = Services.prefs.getCharPref(pref);
      break;
    case Services.prefs.PREF_INT:
      resettable[pref] = Services.prefs.getIntPref(pref);
      break;
    default:
      ok(false, `Unknown pref type for ${pref}`);
    }
  }

  Services.prefs.setStringPref(TP_LIST_PREF, "test-track-simple,base-track-digest256,content-track-digest256");
  Services.prefs.setIntPref(NCB_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);

  for (let pref in resettable) {
    switch (Services.prefs.getPrefType(pref)) {
    case Services.prefs.PREF_STRING:
      // Account for prefs that may have retained their default value
      if (Services.prefs.getCharPref(pref) != resettable[pref]) {
        ok(Services.prefs.prefHasUserValue(pref), `modified the pref ${pref}`);
      }
      break;
    case Services.prefs.PREF_INT:
      if (Services.prefs.getIntPref(pref) != resettable[pref]) {
        ok(Services.prefs.prefHasUserValue(pref), `modified the pref ${pref}`);
      }
      break;
    default:
      ok(false, `Unknown pref type for ${pref}`);
    }
  }

  await extension.startup();

  await TestUtils.waitForCondition(() => Services.prefs.prefHasUserValue(TP_PREF));

  let disabledControls = [
    ".tracking-protection-ui .content-blocking-checkbox",
    "#trackingProtectionMenu",
    "[control=trackingProtectionMenu]",
  ];

  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let doc = gBrowser.contentDocument;

  checkControlState(doc, disabledControls, false);

  let contentBlockingRestoreDefaults = doc.getElementById("contentBlockingRestoreDefaults");
  contentBlockingRestoreDefaults.click();

  for (let pref in resettable) {
    ok(!Services.prefs.prefHasUserValue(pref), `reset the pref ${pref}`);
  }

  ok(Services.prefs.prefHasUserValue(TP_PREF), "did not reset the TP pref");

  await extension.unload();

  gBrowser.removeCurrentTab();
});

function checkControlState(doc, controls, enabled) {
  for (let selector of controls) {
    for (let control of doc.querySelectorAll(selector)) {
      if (enabled) {
        ok(!control.hasAttribute("disabled"), `${selector} is enabled.`);
      } else {
        is(control.getAttribute("disabled"), "true", `${selector} is disabled.`);
      }
    }
  }
}

// Checks that the controls for tracking protection are disabled when all TP prefs are off.
add_task(async function testContentBlockingDependentTPControls() {
  SpecialPowers.pushPrefEnv({set: [
    [CB_TP_UI_PREF, true],
    [CB_RT_UI_PREF, true],
    [TP_PREF, false],
    [TP_PBM_PREF, false],
    [NCB_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER],
  ]});

  let disabledControls = [
    "#trackingProtectionMenu",
  ];

  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let doc = gBrowser.contentDocument;

  checkControlState(doc, disabledControls, false);

  gBrowser.removeCurrentTab();
});

// Checks that the warnings in the Content Blocking Third-Party Cookies section correctly appear based on
// the selections in the Cookies and Site Data section.
add_task(async function testContentBlockingThirdPartyCookiesWarning() {
  await SpecialPowers.pushPrefEnv({set: [
    [CB_TP_UI_PREF, true],
    [CB_RT_UI_PREF, true],
  ]});

  let expectedDeckIndex = new Map([
    [Ci.nsICookieService.BEHAVIOR_ACCEPT, 0],
    [Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN, 0],
    [Ci.nsICookieService.BEHAVIOR_REJECT, 1],
    [Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN, 2],
    [Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER, 0],
  ]);

  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let doc = gBrowser.contentDocument;

  let deck = doc.getElementById("blockCookiesCBDeck");

  for (let obj of expectedDeckIndex) {
    Services.prefs.setIntPref(NCB_PREF, obj[0]);

    is(deck.selectedIndex, obj[1], "Correct deck index is being displayed");

    Services.prefs.clearUserPref(NCB_PREF);
  }

  gBrowser.removeCurrentTab();
});

