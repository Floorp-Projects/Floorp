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

requestLongerTimeout(2);

// Checks that the content blocking toggle follows and changes the CB pref.
add_task(async function testContentBlockingToggle() {
  SpecialPowers.pushPrefEnv({set: [
    [CB_UI_PREF, true],
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

// Tests that the content blocking "Restore Defaults" button does what it's supposed to.
add_task(async function testContentBlockingRestoreDefaults() {
  SpecialPowers.pushPrefEnv({set: [
    [CB_UI_PREF, true],
  ]});

  let prefs = [
    CB_PREF,
    FB_PREF,
    TP_LIST_PREF,
    TP_PREF,
    TP_PBM_PREF,
    NCB_PREF,
  ];

  Services.prefs.setBoolPref(CB_PREF, false);
  Services.prefs.setBoolPref(FB_PREF, true);
  Services.prefs.setStringPref(TP_LIST_PREF, "test-track-simple,base-track-digest256,content-track-digest256");
  Services.prefs.setBoolPref(TP_PREF, true);
  Services.prefs.setBoolPref(TP_PBM_PREF, false);
  Services.prefs.setIntPref(NCB_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);

  for (let pref of prefs) {
    ok(Services.prefs.prefHasUserValue(pref), `modified the pref ${pref}`);
  }

  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let doc = gBrowser.contentDocument;

  let contentBlockingRestoreDefaults = doc.getElementById("contentBlockingRestoreDefaults");
  contentBlockingRestoreDefaults.click();

  // TP prefs are reset async to check for extensions controlling them.
  await TestUtils.waitForCondition(() => !Services.prefs.prefHasUserValue(TP_PREF));

  for (let pref of prefs) {
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

  let resettable = [
    CB_PREF,
    FB_PREF,
    TP_LIST_PREF,
    NCB_PREF,
  ];

  Services.prefs.setBoolPref(CB_PREF, false);
  Services.prefs.setBoolPref(FB_PREF, true);
  Services.prefs.setStringPref(TP_LIST_PREF, "test-track-simple,base-track-digest256,content-track-digest256");
  Services.prefs.setIntPref(NCB_PREF, Ci.nsICookieService.BEHAVIOR_REJECT_TRACKER);

  for (let pref of resettable) {
    ok(Services.prefs.prefHasUserValue(pref), `modified the pref ${pref}`);
  }

  await extension.startup();

  await TestUtils.waitForCondition(() => Services.prefs.prefHasUserValue(TP_PREF));

  await openPreferencesViaOpenPreferencesAPI("privacy", {leaveOpen: true});
  let doc = gBrowser.contentDocument;

  let contentBlockingRestoreDefaults = doc.getElementById("contentBlockingRestoreDefaults");
  contentBlockingRestoreDefaults.click();

  for (let pref of resettable) {
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
    "#content-blocking-categories-label",
    ".content-blocking-checkbox",
    ".content-blocking-icon",
    ".content-blocking-category-name",
    "#changeBlockListLink",
    "#contentBlockingChangeCookieSettings",
  ];
  let alwaysDisabledControls = [
    "#blockCookiesCB, #blockCookiesCB > radio",
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
    "#content-blocking-categories-label",
    ".content-blocking-checkbox",
    ".content-blocking-icon",
    ".content-blocking-category-name",
    "#changeBlockListLink",
    "#contentBlockingChangeCookieSettings",
    "#blockCookiesCB, #blockCookiesCB > radio",
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
    ".content-blocking-checkbox",
    ".content-blocking-icon",
    ".content-blocking-category-name",
    "#changeBlockListLink",
    "#contentBlockingChangeCookieSettings",
    "#blockCookiesCB, #blockCookiesCB > radio",
  ];
  let alwaysDisabledControls = [
    "#trackingProtectionMenu",
    "[control=trackingProtectionMenu]",
  ];

  await doDependentControlChecks(dependentControls, alwaysDisabledControls);
});


// Checks that the granular controls are disabled or enabled depending on the master pref for CB
// when the Cookies and Site Data section is set to block either "All Cookies" or "Cookies from
// unvisited websites".
add_task(async function testContentBlockingDependentControlsOnSiteDataUI() {
  let prefValuesToTest = [
    Ci.nsICookieService.BEHAVIOR_REJECT,        // Block All Cookies
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
    ];
    let alwaysDisabledControls = [
      ".reject-trackers-checkbox",
      ".reject-trackers-icon",
      "[control=blockCookiesCB]",
      "#blockCookiesCBDeck",
      "#blockCookiesCB, #blockCookiesCB > radio",
    ];

    await doDependentControlChecks(dependentControls, alwaysDisabledControls);
  }

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
      ".content-blocking-icon",
      ".content-blocking-category-name",
      "#trackingProtectionMenu",
      "[control=trackingProtectionMenu]",
      "#changeBlockListLink",
      "#contentBlockingChangeCookieSettings",
    ];
    let alwaysDisabledControls = [
      "#blockCookiesCB, #blockCookiesCB > radio",
    ];

    await doDependentControlChecks(dependentControls, alwaysDisabledControls);
  }

  // The rest of the values
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
      ".content-blocking-icon",
      ".content-blocking-category-name",
      "#trackingProtectionMenu",
      "[control=trackingProtectionMenu]",
      "#changeBlockListLink",
      "#contentBlockingChangeCookieSettings",
      "#blockCookiesCB, #blockCookiesCB > radio",
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

