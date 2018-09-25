/* eslint-env webextensions */

const CB_PREF = "browser.contentblocking.enabled";
const CB_UI_PREF = "browser.contentblocking.ui.enabled";
const CB_FB_UI_PREF = "browser.contentblocking.fastblock.ui.enabled";
const CB_TP_UI_PREF = "browser.contentblocking.trackingprotection.ui.enabled";
const CB_RT_UI_PREF = "browser.contentblocking.rejecttrackers.ui.enabled";
const TP_PREF = "privacy.trackingprotection.enabled";
const TP_PBM_PREF = "privacy.trackingprotection.pbmode.enabled";
const TP_LIST_PREF = "urlclassifier.trackingTable";
const FB_PREF = "browser.fastblock.enabled";
const NCB_PREF = "network.cookie.cookieBehavior";
const TOGGLE_PREF = "browser.contentblocking.global-toggle.enabled";

requestLongerTimeout(2);

// Checks that the content blocking toggle follows and changes the CB pref.
add_task(async function testContentBlockingToggle() {
  SpecialPowers.pushPrefEnv({set: [
    [CB_UI_PREF, true],
    [TOGGLE_PREF, true],
  ]});

  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let doc = gBrowser.contentDocument;

  let contentBlockingToggle = doc.getElementById("contentBlockingToggle");
  let contentBlockingCheckbox = doc.getElementById("contentBlockingCheckbox");
  let contentBlockingToggleLabel = doc.getElementById("contentBlockingToggleLabel");

  Services.prefs.setBoolPref(CB_PREF, true);
  await TestUtils.waitForCondition(() => contentBlockingToggleLabel.textContent == "ON", "toggle label is correct");

  ok(contentBlockingCheckbox.checked, "Checkbox is checked when CB is on");
  is(contentBlockingToggle.getAttribute("aria-pressed"), "true", "toggle button has correct aria attribute");

  Services.prefs.setBoolPref(CB_PREF, false);
  await TestUtils.waitForCondition(() => contentBlockingToggleLabel.textContent == "OFF", "toggle label is correct");

  ok(!contentBlockingCheckbox.checked, "Checkbox is not checked when CB is off");
  is(contentBlockingToggle.getAttribute("aria-pressed"), "false", "toggle button has correct aria attribute");

  contentBlockingToggle.click();
  await TestUtils.waitForCondition(() => contentBlockingToggleLabel.textContent == "ON", "toggle label is correct");

  is(Services.prefs.getBoolPref(CB_PREF), true, "Content Blocking is on");
  ok(contentBlockingCheckbox.checked, "Checkbox is checked when CB is on");
  is(contentBlockingToggle.getAttribute("aria-pressed"), "true", "toggle button has correct aria attribute");

  contentBlockingToggle.click();
  await TestUtils.waitForCondition(() => contentBlockingToggleLabel.textContent == "OFF", "toggle label is correct");

  is(Services.prefs.getBoolPref(CB_PREF), false, "Content Blocking is off");
  ok(!contentBlockingCheckbox.checked, "Checkbox is not checked when CB is off");
  is(contentBlockingToggle.getAttribute("aria-pressed"), "false", "toggle button has correct aria attribute");

  Services.prefs.clearUserPref(CB_PREF);
  gBrowser.removeCurrentTab();
});

// Tests that the content blocking main category checkboxes have the correct default state.
add_task(async function testContentBlockingMainCategory() {
  SpecialPowers.pushPrefEnv({set: [
    [CB_UI_PREF, true],
  ]});

  let prefs = [
    [CB_PREF, true],
    [FB_PREF, true],
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
    "#contentBlockingFastBlockCheckbox",
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
  let tpCheckbox = doc.querySelector(checkboxes[1]);

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
  checkControlStateWorker(doc, dependentControls, true);
  checkControlStateWorker(doc, alwaysEnabledControls, true);

  let promise = TestUtils.topicObserved("privacy-pane-tp-ui-updated");
  EventUtils.synthesizeMouseAtCenter(tpCheckbox, {}, doc.defaultView);

  await promise;
  ok(!tpCheckbox.checked, "The checkbox should now be unchecked");

  // Ensure the dependent controls are disabled
  checkControlStateWorker(doc, dependentControls, false);
  checkControlStateWorker(doc, alwaysEnabledControls, true);

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
  SpecialPowers.pushPrefEnv({set: [
    [CB_UI_PREF, true],
  ]});

  let prefs = {
    CB_PREF: null,
    FB_PREF: null,
    TP_LIST_PREF: null,
    TP_PREF: null,
    TP_PBM_PREF: null,
    NCB_PREF: null,
  };

  for (let pref in prefs) {
    switch (Services.prefs.getPrefType(pref)) {
    case Services.prefs.PREF_BOOL:
      prefs[pref] = Services.prefs.getBoolPref(pref);
      break;
    case Services.prefs.PREF_INT:
      prefs[pref] = Services.prefs.getIntPref(pref);
      break;
    }
  }

  Services.prefs.setBoolPref(CB_PREF, false);
  Services.prefs.setBoolPref(FB_PREF, !Services.prefs.getBoolPref(FB_PREF));
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
  SpecialPowers.pushPrefEnv({set: [
    [CB_UI_PREF, true],
  ]});

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
    CB_PREF: null,
    FB_PREF: null,
    TP_LIST_PREF: null,
    NCB_PREF: null,
  };

  for (let pref in resettable) {
    switch (Services.prefs.getPrefType(pref)) {
    case Services.prefs.PREF_BOOL:
      resettable[pref] = Services.prefs.getBoolPref(pref);
      break;
    case Services.prefs.PREF_INT:
      resettable[pref] = Services.prefs.getIntPref(pref);
      break;
    }
  }

  Services.prefs.setBoolPref(CB_PREF, false);
  Services.prefs.setBoolPref(FB_PREF, !Services.prefs.getBoolPref(FB_PREF));
  Services.prefs.setStringPref(TP_LIST_PREF, "test-track-simple,base-track-digest256,content-track-digest256");
  Services.prefs.setIntPref(NCB_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);

  for (let pref in resettable) {
    switch (Services.prefs.getPrefType(pref)) {
    case Services.prefs.PREF_BOOL:
      // Account for prefs that may have retained their default value
      if (Services.prefs.getBoolPref(pref) != resettable[pref]) {
        ok(Services.prefs.prefHasUserValue(pref), `modified the pref ${pref}`);
      }
      break;
    case Services.prefs.PREF_INT:
      if (Services.prefs.getIntPref(pref) != resettable[pref]) {
        ok(Services.prefs.prefHasUserValue(pref), `modified the pref ${pref}`);
      }
      break;
    }
  }

  await extension.startup();

  await TestUtils.waitForCondition(() => Services.prefs.prefHasUserValue(TP_PREF));

  let dependentControls = [
    ".fast-block-ui .content-blocking-checkbox",
    ".reject-trackers-ui .content-blocking-checkbox",
    "#content-blocking-categories-label",
    "#changeBlockListLink",
    "#contentBlockingChangeCookieSettings",
    "#blockCookiesCB, #blockCookiesCB > radio",
  ];
  let alwaysDisabledControls = [
    ".tracking-protection-ui .content-blocking-checkbox",
    "#trackingProtectionMenu",
    "[control=trackingProtectionMenu]",
  ];

  await doDependentControlChecks(dependentControls, alwaysDisabledControls);

  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let doc = gBrowser.contentDocument;

  let contentBlockingRestoreDefaults = doc.getElementById("contentBlockingRestoreDefaults");
  contentBlockingRestoreDefaults.click();

  for (let pref in resettable) {
    ok(!Services.prefs.prefHasUserValue(pref), `reset the pref ${pref}`);
  }

  ok(Services.prefs.prefHasUserValue(TP_PREF), "did not reset the TP pref");

  await extension.unload();

  gBrowser.removeCurrentTab();
});

function checkControlStateWorker(doc, dependentControls, enabled) {
  for (let selector of dependentControls) {
    let controls = doc.querySelectorAll(selector);
    for (let control of controls) {
      if (enabled) {
        ok(!control.hasAttribute("disabled"), `${selector} is enabled because CB is on.`);
      } else {
        is(control.getAttribute("disabled"), "true", `${selector} is disabled because CB is off`);
      }
    }
  }
}

function checkControlState(doc, dependentControls) {
  let enabled = Services.prefs.getBoolPref(CB_PREF);
  return checkControlStateWorker(doc, dependentControls, enabled);
}

async function doDependentControlChecks(dependentControls,
                                        alwaysDisabledControls = []) {
  Services.prefs.setBoolPref(CB_PREF, true);
  Services.prefs.setBoolPref(TOGGLE_PREF, true);

  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let doc = gBrowser.contentDocument;

  is(Services.prefs.getBoolPref(CB_PREF), true, "Content Blocking is on");
  checkControlState(doc, dependentControls);
  checkControlStateWorker(doc, alwaysDisabledControls, false);

  gBrowser.removeCurrentTab();

  Services.prefs.setBoolPref(CB_PREF, false);

  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  doc = gBrowser.contentDocument;

  is(Services.prefs.getBoolPref(CB_PREF), false, "Content Blocking is off");
  checkControlState(doc, dependentControls);
  checkControlStateWorker(doc, alwaysDisabledControls, false);

  let contentBlockingToggle = doc.getElementById("contentBlockingToggle");
  contentBlockingToggle.doCommand();

  await TestUtils.topicObserved("privacy-pane-tp-ui-updated");

  is(Services.prefs.getBoolPref(CB_PREF), true, "Content Blocking is on");
  checkControlState(doc, dependentControls);
  checkControlStateWorker(doc, alwaysDisabledControls, false);

  contentBlockingToggle.doCommand();

  await TestUtils.topicObserved("privacy-pane-tp-ui-updated");

  is(Services.prefs.getBoolPref(CB_PREF), false, "Content Blocking is off");
  checkControlState(doc, dependentControls);
  checkControlStateWorker(doc, alwaysDisabledControls, false);

  Services.prefs.clearUserPref(CB_PREF);
  Services.prefs.clearUserPref(TOGGLE_PREF);
  gBrowser.removeCurrentTab();
}

// Checks that the granular controls are disabled or enabled depending on the master pref for CB.
add_task(async function testContentBlockingDependentControls() {
  // In Accept All Cookies mode, the radiogroup under Third-Party Cookies is always disabled
  // since the checkbox next to Third-Party Cookies would be unchecked.
  SpecialPowers.pushPrefEnv({set: [
    [CB_UI_PREF, true],
    [CB_FB_UI_PREF, true],
    [CB_TP_UI_PREF, true],
    [CB_RT_UI_PREF, true],
    [NCB_PREF, Ci.nsICookieService.BEHAVIOR_ACCEPT],
  ]});

  let dependentControls = [
    ".content-blocking-checkbox",
    "#content-blocking-categories-label",
    "#changeBlockListLink",
    "#contentBlockingChangeCookieSettings",
    "#blockCookies, #blockCookies > radio",
    "#keepUntil",
    "#keepCookiesUntil",
  ];
  let alwaysDisabledControls = [
    "#blockCookiesCB, #blockCookiesCB > radio",
    "#blockCookiesLabel",
    "#blockCookiesMenu",
  ];

  await doDependentControlChecks(dependentControls, alwaysDisabledControls);

  // In Block Cookies from Trackers (or Block Cookies from All Third-Parties) mode, the
  // radiogroup's disabled status must obey the content blocking enabled state.
  SpecialPowers.pushPrefEnv({set: [
    [CB_UI_PREF, true],
    [CB_FB_UI_PREF, true],
    [CB_TP_UI_PREF, true],
    [CB_RT_UI_PREF, true],
    [NCB_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER],
  ]});

  dependentControls = [
    ".content-blocking-checkbox",
    "#content-blocking-categories-label",
    "#changeBlockListLink",
    "#contentBlockingChangeCookieSettings",
    "#blockCookiesCB, #blockCookiesCB > radio",
    "#blockCookies, #blockCookies > radio",
    "#blockCookiesLabel",
    "#blockCookiesMenu",
    "#keepUntil",
    "#keepCookiesUntil",
  ];

  await doDependentControlChecks(dependentControls);
});

// Checks that the controls for tracking protection are disabled when all TP prefs are off.
add_task(async function testContentBlockingDependentTPControls() {
  SpecialPowers.pushPrefEnv({set: [
    [CB_UI_PREF, true],
    [CB_FB_UI_PREF, true],
    [CB_TP_UI_PREF, true],
    [CB_RT_UI_PREF, true],
    [TP_PREF, false],
    [TP_PBM_PREF, false],
    [NCB_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER],
  ]});

  let dependentControls = [
    "#content-blocking-categories-label",
    "[control=trackingProtectionMenu]",
    "#changeBlockListLink",
    "#contentBlockingChangeCookieSettings",
    "#blockCookiesCB, #blockCookiesCB > radio",
    "#blockCookies, #blockCookies > radio",
    "#blockCookiesLabel",
    "#blockCookiesMenu",
    "#keepUntil",
    "#keepCookiesUntil",
  ];
  let alwaysDisabledControls = [
    "#trackingProtectionMenu",
  ];

  await doDependentControlChecks(dependentControls, alwaysDisabledControls);
});


// Checks that the granular controls are disabled or enabled depending on the master pref for CB
// when the Cookies and Site Data section is set to block either "All Cookies" or "Cookies from
// unvisited websites".
add_task(async function testContentBlockingDependentControlsOnSiteDataUI() {
  let prefValuesToTest = [
    Ci.nsICookieService.BEHAVIOR_REJECT,        // Block All Cookies
  ];
  for (let value of prefValuesToTest) {
    await SpecialPowers.pushPrefEnv({set: [
      [CB_UI_PREF, true],
      [CB_FB_UI_PREF, true],
      [CB_TP_UI_PREF, true],
      [CB_RT_UI_PREF, true],
      [TP_PREF, false],
      [TP_PBM_PREF, true],
      [NCB_PREF, value],
    ]});

    // When Block All Cookies is selected, the Third-Party Cookies section under Content Blocking
    // as well as the Keep Until controls under Cookies and Site Data should get disabled
    // unconditionally.
    let dependentControls = [
      "#content-blocking-categories-label",
      "#contentBlockingFastBlockCheckbox",
      "#contentBlockingTrackingProtectionCheckbox",
      ".fastblock-icon",
      ".tracking-protection-icon",
      "#trackingProtectionMenu",
      "[control=trackingProtectionMenu]",
      "#changeBlockListLink",
      "#contentBlockingChangeCookieSettings",
      "#blockCookies, #blockCookies > radio",
      "#blockCookiesLabel",
      "#blockCookiesMenu",
    ];
    let alwaysDisabledControls = [
      "[control=blockCookiesCB]",
      "#blockCookiesCBDeck",
      "#blockCookiesCB, #blockCookiesCB > radio",
      "#keepUntil",
      "#keepCookiesUntil",
    ];

    await doDependentControlChecks(dependentControls, alwaysDisabledControls);
  }

  // When Block Cookies from unvisited websites is selected, the Third-Party Cookies section under
  // Content Blocking should get disabled unconditionally.
  prefValuesToTest = [
    Ci.nsICookieService.BEHAVIOR_LIMIT_FOREIGN, // Block Cookies from unvisited websites
  ];
  for (let value of prefValuesToTest) {
    await SpecialPowers.pushPrefEnv({set: [
      [CB_UI_PREF, true],
      [CB_FB_UI_PREF, true],
      [CB_TP_UI_PREF, true],
      [CB_RT_UI_PREF, true],
      [TP_PREF, false],
      [TP_PBM_PREF, true],
      [NCB_PREF, value],
    ]});

    let dependentControls = [
      "#content-blocking-categories-label",
      "#contentBlockingFastBlockCheckbox",
      "#contentBlockingTrackingProtectionCheckbox",
      ".fastblock-icon",
      ".tracking-protection-icon",
      "#trackingProtectionMenu",
      "[control=trackingProtectionMenu]",
      "#changeBlockListLink",
      "#contentBlockingChangeCookieSettings",
      "#blockCookies, #blockCookies > radio",
      "#blockCookiesLabel",
      "#blockCookiesMenu",
      "#keepUntil",
      "#keepCookiesUntil",
    ];
    let alwaysDisabledControls = [
      "[control=blockCookiesCB]",
      "#blockCookiesCBDeck",
      "#blockCookiesCB, #blockCookiesCB > radio",
    ];

    await doDependentControlChecks(dependentControls, alwaysDisabledControls);
  }

  // When Accept All Cookies is selected, the radio buttons under Third-Party Cookies
  // in Content Blocking as well as the Type blocked controls in Cookies and Site Data
  // must remain disabled unconditionally.
  prefValuesToTest = [
    Ci.nsICookieService.BEHAVIOR_ACCEPT,         // Accept All Cookies
  ];
  for (let value of prefValuesToTest) {
    await SpecialPowers.pushPrefEnv({set: [
      [CB_UI_PREF, true],
      [CB_FB_UI_PREF, true],
      [CB_TP_UI_PREF, true],
      [CB_RT_UI_PREF, true],
      [TP_PREF, false],
      [TP_PBM_PREF, true],
      [NCB_PREF, value],
    ]});

    let dependentControls = [
      "#content-blocking-categories-label",
      ".content-blocking-checkbox",
      "#trackingProtectionMenu",
      "[control=trackingProtectionMenu]",
      "#changeBlockListLink",
      "#contentBlockingChangeCookieSettings",
      "#blockCookies, #blockCookies > radio",
      "#keepUntil",
      "#keepCookiesUntil",
    ];
    let alwaysDisabledControls = [
      "#blockCookiesCB, #blockCookiesCB > radio",
      "#blockCookiesLabel",
      "#blockCookiesMenu",
    ];

    await doDependentControlChecks(dependentControls, alwaysDisabledControls);
  }

  // For other choices of cookie policies, no parts of the UI should get disabled
  // unconditionally.
  prefValuesToTest = [
    Ci.nsICookieService.BEHAVIOR_REJECT_FOREIGN, // Block All Third-Party Cookies
    Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER, // Block Cookies from third-party trackers
  ];
  for (let value of prefValuesToTest) {
    await SpecialPowers.pushPrefEnv({set: [
      [CB_UI_PREF, true],
      [CB_FB_UI_PREF, true],
      [CB_TP_UI_PREF, true],
      [CB_RT_UI_PREF, true],
      [TP_PREF, false],
      [TP_PBM_PREF, true],
      [NCB_PREF, value],
    ]});

    let dependentControls = [
      "#content-blocking-categories-label",
      ".content-blocking-checkbox",
      "#trackingProtectionMenu",
      "[control=trackingProtectionMenu]",
      "#changeBlockListLink",
      "#contentBlockingChangeCookieSettings",
      "#blockCookiesCB, #blockCookiesCB > radio",
      "#blockCookies, #blockCookies > radio",
      "#blockCookiesLabel",
      "#blockCookiesMenu",
      "#keepUntil",
      "#keepCookiesUntil",
    ];

    await doDependentControlChecks(dependentControls);
  }
});


// Checks that the warnings in the Content Blocking Third-Party Cookies section correctly appear based on
// the selections in the Cookies and Site Data section.
add_task(async function testContentBlockingThirdPartyCookiesWarning() {
  await SpecialPowers.pushPrefEnv({set: [
    [CB_PREF, true],
    [CB_UI_PREF, true],
    [CB_FB_UI_PREF, true],
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

