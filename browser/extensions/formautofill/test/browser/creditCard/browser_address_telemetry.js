"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

// Preference definitions
const ENABLED_PREF = ENABLED_AUTOFILL_ADDRESSES_PREF;
const AVAILABLE_PREF = AUTOFILL_ADDRESSES_AVAILABLE_PREF;
const CAPTURE_ENABLE_PREF = ENABLED_AUTOFILL_ADDRESSES_CAPTURE_PREF;

// Telemetry definitions
const EVENT_CATEGORY = "address";

const SCALAR_DETECTED_SECTION_COUNT =
  "formautofill.addresses.detected_sections_count";
const SCALAR_SUBMITTED_SECTION_COUNT =
  "formautofill.addresses.submitted_sections_count";
const SCALAR_AUTOFILL_PROFILE_COUNT =
  "formautofill.addresses.autofill_profiles_count";

const HISTOGRAM_PROFILE_NUM_USES = "AUTOFILL_PROFILE_NUM_USES";
const HISTOGRAM_PROFILE_NUM_USES_KEY = "address";

// Autofill UI
const MANAGE_DIALOG_URL = MANAGE_ADDRESSES_DIALOG_URL; // MANAGE_CREDIT_CARDS_DIALOG_URL
const EDIT_DIALOG_URL = EDIT_ADDRESS_DIALOG_URL; // MANAGE_CREDIT_CARDS_DIALOG_URL
const DIALOG_SIZE = "width=600,height=400";
const MANAGE_RECORD_SELECTOR = "#addresses"; // "#credit-cards";

// Test specific definitions
const TEST_PROFILE = TEST_ADDRESS_1; // TEST_CREDIT_CARD_1
const TEST_PROFILE_1 = TEST_ADDRESS_1; // TEST_CREDIT_CARD_1
const TEST_PROFILE_2 = TEST_ADDRESS_2; // TEST_CREDIT_CARD_1
const TEST_PROFILE_3 = TEST_ADDRESS_3; // TEST_CREDIT_CARD_1
const TEST_BASIC_AUTOFILL_FORM_URL = ADDRESS_FORM_URL; // CREDITCARD_FORM_URL
const TEST_WITHOUT_AUTOCOMPLETE_URL = ADDRESS_FORM_WITHOUT_AUTOCOMPLETE_URL;
const TEST_FOCUS_FIELD = "given-name";
const TEST_FOCUS_SELECTOR = "#" + TEST_FOCUS_FIELD; // "#cc-number"

const TEST_NEW_VALUES = {
  "#given-name": "Test User",
  "#organization": "Sesame Street",
  "#street-address": "123 Sesame Street",
  "#tel": "1-345-345-3456",
};

function getProfiles() {
  return getAddresses();
}

async function assertTelemetry(expected_content, expected_parent) {
  let snapshots;

  info(
    `Waiting for ${expected_content?.length ?? 0} content events and ` +
      `${expected_parent?.length ?? 0} parent events`
  );

  await TestUtils.waitForCondition(
    () => {
      snapshots = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      );

      return (
        (snapshots.parent?.length ?? 0) >= (expected_parent?.length ?? 0) &&
        (snapshots.content?.length ?? 0) >= (expected_content?.length ?? 0)
      );
    },
    "Wait for telemetry to be collected",
    100,
    100
  );

  info(JSON.stringify(snapshots, null, 2));

  if (expected_content !== undefined) {
    expected_content = expected_content.map(
      ([category, method, object, value, extra]) => {
        return { category, method, object, value, extra };
      }
    );

    let clear = expected_parent === undefined;

    TelemetryTestUtils.assertEvents(
      expected_content,
      {
        category: EVENT_CATEGORY,
      },
      { clear, process: "content" }
    );
  }

  if (expected_parent !== undefined) {
    expected_parent = expected_parent.map(
      ([category, method, object, value, extra]) => {
        return { category, method, object, value, extra };
      }
    );
    TelemetryTestUtils.assertEvents(
      expected_parent,
      {
        category: EVENT_CATEGORY,
      },
      { process: "parent" }
    );
  }
}

function _assertHistogram(snapshot, expectedNonZeroRanges) {
  // Compute the actual ranges in the format { range1: value1, range2: value2 }.
  let actualNonZeroRanges = {};
  for (let [range, value] of Object.entries(snapshot.values)) {
    if (value > 0) {
      actualNonZeroRanges[range] = value;
    }
  }

  // These are stringified to visualize the differences between the values.
  Assert.equal(
    JSON.stringify(actualNonZeroRanges),
    JSON.stringify(expectedNonZeroRanges)
  );
}

function assertKeyedHistogram(histogramId, key, expected) {
  let snapshot = Services.telemetry
    .getKeyedHistogramById(histogramId)
    .snapshot();

  if (expected == undefined) {
    Assert.deepEqual(snapshot, {});
  } else {
    _assertHistogram(snapshot[key], expected);
  }
}

async function openTabAndUseAutofillProfile(
  idx,
  profile,
  { closeTab = true, submitForm = true } = {}
) {
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    TEST_BASIC_AUTOFILL_FORM_URL
  );
  let browser = tab.linkedBrowser;

  await openPopupOn(browser, "form " + TEST_FOCUS_SELECTOR);

  for (let i = 0; i <= idx; i++) {
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
  }

  await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
  await waitForAutofill(
    browser,
    TEST_FOCUS_SELECTOR,
    profile[TEST_FOCUS_FIELD]
  );
  await focusUpdateSubmitForm(
    browser,
    {
      focusSelector: TEST_FOCUS_SELECTOR,
      newValues: {},
    },
    submitForm
  );

  if (!closeTab) {
    return tab;
  }

  await BrowserTestUtils.removeTab(tab);
  return null;
}

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [ENABLED_PREF, true],
      [AVAILABLE_PREF, "on"],
      [CAPTURE_ENABLE_PREF, true],
    ],
  });

  Services.telemetry.setEventRecordingEnabled(EVENT_CATEGORY, true);

  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();

  registerCleanupFunction(() => {
    Services.telemetry.setEventRecordingEnabled(EVENT_CATEGORY, false);
  });
});

add_task(async function test_popup_opened() {
  await setStorage(TEST_PROFILE);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: TEST_BASIC_AUTOFILL_FORM_URL },
    async function(browser) {
      const focusInput = TEST_FOCUS_SELECTOR;

      await openPopupOn(browser, focusInput);

      // Clean up
      await closePopup(browser);
    }
  );

  await assertTelemetry([
    // TODO: Implement form interaction event telemetry
  ]);

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("content"),
    SCALAR_DETECTED_SECTION_COUNT,
    1,
    "There should be 1 section detected."
  );
  TelemetryTestUtils.assertScalarUnset(
    TelemetryTestUtils.getProcessScalars("content"),
    SCALAR_SUBMITTED_SECTION_COUNT,
    1
  );

  await removeAllRecords();
  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
});

add_task(async function test_popup_opened_form_without_autocomplete() {
  await setStorage(TEST_PROFILE);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: TEST_WITHOUT_AUTOCOMPLETE_URL },
    async function(browser) {
      const focusInput = TEST_FOCUS_SELECTOR;
      await openPopupOn(browser, focusInput);
      await closePopup(browser);
    }
  );

  await assertTelemetry([
    //TODO: Implement form interaction event telemetry
  ]);

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("content"),
    SCALAR_DETECTED_SECTION_COUNT,
    1,
    "There should be 1 section detected."
  );
  TelemetryTestUtils.assertScalarUnset(
    TelemetryTestUtils.getProcessScalars("content"),
    SCALAR_SUBMITTED_SECTION_COUNT
  );

  await removeAllRecords();
  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
});

add_task(async function test_submit_autofill_profile_new() {
  async function test_per_command(
    command,
    idx,
    useCount = {},
    expectChanged = undefined
  ) {
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: TEST_BASIC_AUTOFILL_FORM_URL },
      async function(browser) {
        let onPopupShown = waitForPopupShown();
        let onChanged;
        if (expectChanged !== undefined) {
          onChanged = TestUtils.topicObserved("formautofill-storage-changed");
        }

        await focusUpdateSubmitForm(browser, {
          focusSelector: TEST_FOCUS_SELECTOR,
          newValues: TEST_NEW_VALUES,
        });

        await onPopupShown;
        await clickDoorhangerButton(command, idx);
        if (expectChanged !== undefined) {
          await onChanged;
          TelemetryTestUtils.assertScalar(
            TelemetryTestUtils.getProcessScalars("parent"),
            SCALAR_AUTOFILL_PROFILE_COUNT,
            expectChanged,
            "There should be ${expectChanged} profile(s) stored."
          );
        }
      }
    );

    assertKeyedHistogram(
      HISTOGRAM_PROFILE_NUM_USES,
      HISTOGRAM_PROFILE_NUM_USES_KEY,
      useCount
    );

    await removeAllRecords();
  }

  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
  Services.telemetry.getKeyedHistogramById(HISTOGRAM_PROFILE_NUM_USES).clear();

  let expected_content = [
    // TODO: Implement form interaction event telemetry
  ];

  // FTU
  await test_per_command(MAIN_BUTTON, undefined, { 1: 1 }, 1);
  await assertTelemetry(expected_content, [
    [EVENT_CATEGORY, "show", "capture_doorhanger"],
    [EVENT_CATEGORY, "pref", "capture_doorhanger"],
  ]);

  // Need to close preference tab
  BrowserTestUtils.removeTab(gBrowser.selectedTab);

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("content"),
    SCALAR_DETECTED_SECTION_COUNT,
    1,
    "There should be 1 sections detected."
  );
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("content"),
    SCALAR_SUBMITTED_SECTION_COUNT,
    1,
    "There should be 1 section submitted."
  );

  await removeAllRecords();
  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
});

add_task(async function test_submit_autofill_profile_update() {
  async function test_per_command(
    command,
    idx,
    useCount = {},
    expectChanged = undefined
  ) {
    await setStorage(TEST_PROFILE);
    let profiles = await getProfiles();
    Assert.equal(profiles.length, 1, "1 entry in storage");

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: TEST_BASIC_AUTOFILL_FORM_URL },
      async function(browser) {
        let onPopupShown = waitForPopupShown();
        let onChanged;
        if (expectChanged !== undefined) {
          onChanged = TestUtils.topicObserved("formautofill-storage-changed");
        }

        await openPopupOn(browser, TEST_FOCUS_SELECTOR);
        await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
        await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);

        await waitForAutofill(
          browser,
          TEST_FOCUS_SELECTOR,
          TEST_PROFILE[TEST_FOCUS_FIELD]
        );
        await focusUpdateSubmitForm(browser, {
          focusSelector: TEST_FOCUS_SELECTOR,
          newValues: TEST_NEW_VALUES,
        });
        await onPopupShown;
        await clickDoorhangerButton(command, idx);
        if (expectChanged !== undefined) {
          await onChanged;
          TelemetryTestUtils.assertScalar(
            TelemetryTestUtils.getProcessScalars("parent"),
            SCALAR_AUTOFILL_PROFILE_COUNT,
            expectChanged,
            "There should be ${expectChanged} profile(s) stored."
          );
        }
      }
    );

    assertKeyedHistogram(
      HISTOGRAM_PROFILE_NUM_USES,
      HISTOGRAM_PROFILE_NUM_USES_KEY,
      useCount
    );

    SpecialPowers.clearUserPref(ENABLED_PREF);

    await removeAllRecords();
  }
  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
  Services.telemetry.getKeyedHistogramById(HISTOGRAM_PROFILE_NUM_USES).clear();

  let expected_content = [];

  await test_per_command(MAIN_BUTTON, undefined, { 1: 1 }, 1);
  await assertTelemetry(expected_content, [
    [EVENT_CATEGORY, "show", "update_doorhanger"],
    [EVENT_CATEGORY, "update", "update_doorhanger"],
  ]);

  await test_per_command(SECONDARY_BUTTON, undefined, { 0: 1, 1: 1 }, 2);
  await assertTelemetry(expected_content, [
    [EVENT_CATEGORY, "show", "update_doorhanger"],
    [EVENT_CATEGORY, "save", "update_doorhanger"],
  ]);

  await removeAllRecords();
  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
});

add_task(async function test_removingAutofillProfilesViaKeyboardDelete() {
  await setStorage(TEST_PROFILE);

  let win = window.openDialog(MANAGE_DIALOG_URL, null, DIALOG_SIZE);
  await waitForFocusAndFormReady(win);

  let selRecords = win.document.querySelector(MANAGE_RECORD_SELECTOR);
  Assert.equal(selRecords.length, 1, "One entry");

  EventUtils.synthesizeMouseAtCenter(selRecords.children[0], {}, win);
  EventUtils.synthesizeKey("VK_DELETE", {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsRemoved");
  Assert.equal(selRecords.length, 0, "No entry left");

  win.close();

  await assertTelemetry(undefined, [
    [EVENT_CATEGORY, "show", "manage"],
    [EVENT_CATEGORY, "delete", "manage"],
  ]);

  await removeAllRecords();
  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
});

add_task(async function test_saveAutofillProfile() {
  Services.telemetry.clearEvents();

  await testDialog(EDIT_DIALOG_URL, win => {
    // TODP: Default to US because the layout will be different
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_PROFILE["given-name"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_PROFILE["family-name"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_PROFILE["street-address"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_PROFILE["address-level2"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_PROFILE["postal-code"], {}, win);
    info("saving one entry");
    win.document.querySelector("#save").click();
  });

  await assertTelemetry(undefined, [[EVENT_CATEGORY, "add", "manage"]]);

  await removeAllRecords();
  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
});

add_task(async function test_editAutofillProfile() {
  Services.telemetry.clearEvents();

  await setStorage(TEST_PROFILE);

  let profiles = await getProfiles();
  Assert.equal(profiles.length, 1, "1 entry in storage");
  await testDialog(
    EDIT_DIALOG_URL,
    win => {
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_RIGHT", {}, win);
      EventUtils.synthesizeKey("test", {}, win);
      win.document.querySelector("#save").click();
    },
    {
      record: profiles[0],
    }
  );

  await assertTelemetry(undefined, [
    [EVENT_CATEGORY, "show_entry", "manage"],
    [EVENT_CATEGORY, "edit", "manage"],
  ]);

  await removeAllRecords();
  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
});

add_task(async function test_histogram() {
  Services.telemetry.getKeyedHistogramById(HISTOGRAM_PROFILE_NUM_USES).clear();

  await setStorage(TEST_PROFILE_1, TEST_PROFILE_2, TEST_PROFILE_3);
  let profiles = await getProfiles();
  Assert.equal(profiles.length, 3, "3 entry in storage");

  assertKeyedHistogram(
    HISTOGRAM_PROFILE_NUM_USES,
    HISTOGRAM_PROFILE_NUM_USES_KEY,
    { 0: 3 }
  );

  await openTabAndUseAutofillProfile(0, TEST_PROFILE_1);
  assertKeyedHistogram(
    HISTOGRAM_PROFILE_NUM_USES,
    HISTOGRAM_PROFILE_NUM_USES_KEY,
    { 0: 2, 1: 1 }
  );

  await openTabAndUseAutofillProfile(1, TEST_PROFILE_2);
  assertKeyedHistogram(
    HISTOGRAM_PROFILE_NUM_USES,
    HISTOGRAM_PROFILE_NUM_USES_KEY,
    { 0: 1, 1: 2 }
  );

  await openTabAndUseAutofillProfile(0, TEST_PROFILE_2);
  assertKeyedHistogram(
    HISTOGRAM_PROFILE_NUM_USES,
    HISTOGRAM_PROFILE_NUM_USES_KEY,
    { 0: 1, 1: 1, 2: 1 }
  );

  await openTabAndUseAutofillProfile(1, TEST_PROFILE_1);
  assertKeyedHistogram(
    HISTOGRAM_PROFILE_NUM_USES,
    HISTOGRAM_PROFILE_NUM_USES_KEY,
    { 0: 1, 2: 2 }
  );

  await openTabAndUseAutofillProfile(2, TEST_PROFILE_3);
  assertKeyedHistogram(
    HISTOGRAM_PROFILE_NUM_USES,
    HISTOGRAM_PROFILE_NUM_USES_KEY,
    { 1: 1, 2: 2 }
  );

  await removeAllRecords();

  assertKeyedHistogram(
    HISTOGRAM_PROFILE_NUM_USES,
    HISTOGRAM_PROFILE_NUM_USES_KEY,
    undefined
  );

  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
});

add_task(async function test_clear_autofill_profile_autofill() {
  // TODO: Support field detection telemetry
  Assert.ok(true);
});
