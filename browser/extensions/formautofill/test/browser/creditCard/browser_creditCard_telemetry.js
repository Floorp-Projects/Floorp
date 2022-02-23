/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const CC_NUM_USES_HISTOGRAM = "CREDITCARD_NUM_USES";

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
        category: "creditcard",
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
        category: "creditcard",
      },
      { process: "parent" }
    );
  }
}

function assertHistogram(histogramId, expectedNonZeroRanges) {
  let snapshot = Services.telemetry.getHistogramById(histogramId).snapshot();

  // Compute the actual ranges in the format { range1: value1, range2: value2 }.
  let actualNonZeroRanges = {};
  for (let [range, value] of Object.entries(snapshot.values)) {
    if (value > 0) {
      actualNonZeroRanges[range] = value;
    }
  }

  // These are stringified to visualize the differences between the values.
  info("Testing histogram: " + histogramId);
  Assert.equal(
    JSON.stringify(actualNonZeroRanges),
    JSON.stringify(expectedNonZeroRanges)
  );
}

async function useCreditCard(idx) {
  let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  let onUsed = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => data == "notifyUsed"
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      await openPopupOn(browser, "form #cc-name");
      for (let i = 0; i < idx; i++) {
        await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      }
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await osKeyStoreLoginShown;
      await SpecialPowers.spawn(browser, [], async function() {
        await ContentTaskUtils.waitForCondition(() => {
          let form = content.document.getElementById("form");
          let number = form.querySelector("#cc-number");
          return !!number.value.length;
        }, "Credit card detail never fills");
        let form = content.document.getElementById("form");

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await sleep(1000);
      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
    }
  );
  await onUsed;
}

add_task(async function test_popup_opened() {
  await SpecialPowers.pushPrefEnv({
    set: [
      [AUTOFILL_CREDITCARDS_AVAILABLE_PREF, true],
      [ENABLED_AUTOFILL_CREDITCARDS_PREF, true],
    ],
  });

  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

  await saveCreditCard(TEST_CREDIT_CARD_1);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      const focusInput = "#cc-number";

      await openPopupOn(browser, focusInput);

      // Clean up
      await closePopup(browser);
    }
  );

  await removeAllRecords();

  await assertTelemetry([
    ["creditcard", "detected", "cc_form"],
    ["creditcard", "popup_shown", "cc_form"],
  ]);

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("content"),
    "formautofill.creditCards.detected_sections_count",
    1,
    "There should be 1 section detected."
  );
  TelemetryTestUtils.assertScalarUnset(
    TelemetryTestUtils.getProcessScalars("content"),
    "formautofill.creditCards.submitted_sections_count"
  );

  SpecialPowers.clearUserPref(AUTOFILL_CREDITCARDS_AVAILABLE_PREF);
  SpecialPowers.clearUserPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);
});

add_task(async function test_submit_creditCard_new() {
  async function test_per_command(
    command,
    idx,
    useCount = {},
    expectChanged = undefined
  ) {
    await SpecialPowers.pushPrefEnv({
      set: [
        [CREDITCARDS_USED_STATUS_PREF, 0],
        [AUTOFILL_CREDITCARDS_AVAILABLE_PREF, true],
        [ENABLED_AUTOFILL_CREDITCARDS_PREF, true],
      ],
    });
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: CREDITCARD_FORM_URL },
      async function(browser) {
        let promiseShown = BrowserTestUtils.waitForEvent(
          PopupNotifications.panel,
          "popupshown"
        );
        let onChanged = TestUtils.topicObserved("formautofill-storage-changed");

        await SpecialPowers.spawn(browser, [], async function() {
          let form = content.document.getElementById("form");
          let name = form.querySelector("#cc-name");

          name.focus();
          name.setUserInput("User 1");

          form.querySelector("#cc-number").setUserInput("5038146897157463");
          form.querySelector("#cc-exp-month").setUserInput("12");
          form.querySelector("#cc-exp-year").setUserInput("2017");
          form.querySelector("#cc-type").value = "mastercard";

          // Wait 1000ms before submission to make sure the input value applied
          await new Promise(resolve => content.setTimeout(resolve, 1000));
          form.querySelector("input[type=submit]").click();
        });

        await promiseShown;
        await clickDoorhangerButton(command, idx);
        if (expectChanged !== undefined) {
          await onChanged;
          TelemetryTestUtils.assertScalar(
            TelemetryTestUtils.getProcessScalars("parent"),
            "formautofill.creditCards.autofill_profiles_count",
            expectChanged,
            "There should be ${expectChanged} profile(s) stored."
          );
        }
      }
    );

    SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
    SpecialPowers.clearUserPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);
    SpecialPowers.clearUserPref(AUTOFILL_CREDITCARDS_AVAILABLE_PREF);

    assertHistogram(CC_NUM_USES_HISTOGRAM, useCount);

    await removeAllRecords();
  }

  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
  Services.telemetry.getHistogramById(CC_NUM_USES_HISTOGRAM).clear();
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

  let expected_content = [
    ["creditcard", "detected", "cc_form"],
    [
      "creditcard",
      "submitted",
      "cc_form",
      undefined,
      {
        // 5 fields plus submit button
        fields_not_auto: "6",
        fields_auto: "0",
        fields_modified: "0",
      },
    ],
  ];
  await test_per_command(MAIN_BUTTON, undefined, { 1: 1 }, 1);
  await assertTelemetry(expected_content, [
    ["creditcard", "show", "capture_doorhanger"],
    ["creditcard", "save", "capture_doorhanger"],
  ]);

  await test_per_command(SECONDARY_BUTTON);
  await assertTelemetry(expected_content, [
    ["creditcard", "show", "capture_doorhanger"],
    ["creditcard", "cancel", "capture_doorhanger"],
  ]);

  await test_per_command(MENU_BUTTON, 0);
  await assertTelemetry(expected_content, [
    ["creditcard", "show", "capture_doorhanger"],
    ["creditcard", "disable", "capture_doorhanger"],
  ]);

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("content"),
    "formautofill.creditCards.detected_sections_count",
    3,
    "There should be 3 sections detected."
  );
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("content"),
    "formautofill.creditCards.submitted_sections_count",
    3,
    "There should be 1 section submitted."
  );
});

add_task(async function test_submit_creditCard_autofill() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  Services.telemetry.clearEvents();
  Services.telemetry.getHistogramById(CC_NUM_USES_HISTOGRAM).clear();
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

  await SpecialPowers.pushPrefEnv({
    set: [
      [CREDITCARDS_USED_STATUS_PREF, 0],
      [AUTOFILL_CREDITCARDS_AVAILABLE_PREF, true],
      [ENABLED_AUTOFILL_CREDITCARDS_PREF, true],
    ],
  });

  await saveCreditCard(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  await useCreditCard(1);

  assertHistogram(CC_NUM_USES_HISTOGRAM, {
    1: 1,
  });

  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  SpecialPowers.clearUserPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);
  SpecialPowers.clearUserPref(AUTOFILL_CREDITCARDS_AVAILABLE_PREF);

  await removeAllRecords();

  await assertTelemetry(
    [
      ["creditcard", "detected", "cc_form"],
      ["creditcard", "popup_shown", "cc_form"],
      ["creditcard", "filled", "cc_form"],
      [
        "creditcard",
        "submitted",
        "cc_form",
        undefined,
        {
          fields_not_auto: "3",
          fields_auto: "5",
          fields_modified: "0",
        },
      ],
    ],
    []
  );
});

add_task(async function test_submit_creditCard_update() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  async function test_per_command(
    command,
    idx,
    useCount = {},
    expectChanged = undefined
  ) {
    await SpecialPowers.pushPrefEnv({
      set: [
        [CREDITCARDS_USED_STATUS_PREF, 0],
        [AUTOFILL_CREDITCARDS_AVAILABLE_PREF, true],
        [ENABLED_AUTOFILL_CREDITCARDS_PREF, true],
      ],
    });

    await saveCreditCard(TEST_CREDIT_CARD_1);
    let creditCards = await getCreditCards();
    is(creditCards.length, 1, "1 credit card in storage");

    let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: CREDITCARD_FORM_URL },
      async function(browser) {
        let promiseShown = BrowserTestUtils.waitForEvent(
          PopupNotifications.panel,
          "popupshown"
        );
        let onChanged = TestUtils.topicObserved("formautofill-storage-changed");

        await openPopupOn(browser, "form #cc-name");
        await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
        await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
        await osKeyStoreLoginShown;
        await SpecialPowers.spawn(browser, [], async function() {
          await ContentTaskUtils.waitForCondition(() => {
            let form = content.document.getElementById("form");
            let name = form.querySelector("#cc-name");
            return name.value == "John Doe";
          }, "Credit card detail never fills");
          let form = content.document.getElementById("form");
          let year = form.querySelector("#cc-exp-year");
          year.setUserInput("2019");

          // Wait 1000ms before submission to make sure the input value applied
          await new Promise(resolve => content.setTimeout(resolve, 1000));
          form.querySelector("input[type=submit]").click();
        });
        await promiseShown;
        await clickDoorhangerButton(command, idx);
        if (expectChanged !== undefined) {
          await onChanged;
          TelemetryTestUtils.assertScalar(
            TelemetryTestUtils.getProcessScalars("parent"),
            "formautofill.creditCards.autofill_profiles_count",
            expectChanged,
            "There should be ${expectChanged} profile(s) stored."
          );
        }
      }
    );

    assertHistogram("CREDITCARD_NUM_USES", useCount);

    SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
    SpecialPowers.clearUserPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);
    SpecialPowers.clearUserPref(AUTOFILL_CREDITCARDS_AVAILABLE_PREF);

    await removeAllRecords();
  }
  Services.telemetry.clearEvents();
  Services.telemetry.getHistogramById(CC_NUM_USES_HISTOGRAM).clear();
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

  let expected_content = [
    ["creditcard", "detected", "cc_form"],
    ["creditcard", "popup_shown", "cc_form"],
    ["creditcard", "filled", "cc_form"],
    [
      "creditcard",
      "filled_modified",
      "cc_form",
      undefined,
      { field_name: "cc-exp-year" },
    ],
    [
      "creditcard",
      "submitted",
      "cc_form",
      undefined,
      {
        fields_not_auto: "3",
        fields_auto: "5",
        fields_modified: "1",
      },
    ],
  ];

  await test_per_command(MAIN_BUTTON, undefined, { 1: 1 }, 1);
  await assertTelemetry(expected_content, [
    ["creditcard", "show", "update_doorhanger"],
    ["creditcard", "update", "update_doorhanger"],
  ]);

  await test_per_command(SECONDARY_BUTTON, undefined, { 0: 1, 1: 1 }, 2);
  await assertTelemetry(expected_content, [
    ["creditcard", "show", "update_doorhanger"],
    ["creditcard", "save", "update_doorhanger"],
  ]);
});

const TEST_SELECTORS = {
  selRecords: "#credit-cards",
  btnRemove: "#remove",
  btnAdd: "#add",
  btnEdit: "#edit",
};

const DIALOG_SIZE = "width=600,height=400";

add_task(async function test_removingCreditCardsViaKeyboardDelete() {
  Services.telemetry.clearEvents();
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

  await SpecialPowers.pushPrefEnv({
    set: [
      [AUTOFILL_CREDITCARDS_AVAILABLE_PREF, true],
      [ENABLED_AUTOFILL_CREDITCARDS_PREF, true],
    ],
  });

  await saveCreditCard(TEST_CREDIT_CARD_1);
  let win = window.openDialog(
    MANAGE_CREDIT_CARDS_DIALOG_URL,
    null,
    DIALOG_SIZE
  );
  await waitForFocusAndFormReady(win);

  let selRecords = win.document.querySelector(TEST_SELECTORS.selRecords);

  is(selRecords.length, 1, "One credit card");

  EventUtils.synthesizeMouseAtCenter(selRecords.children[0], {}, win);
  EventUtils.synthesizeKey("VK_DELETE", {}, win);
  await BrowserTestUtils.waitForEvent(selRecords, "RecordsRemoved");
  is(selRecords.length, 0, "No credit cards left");

  win.close();

  await assertTelemetry(undefined, [
    ["creditcard", "show", "manage"],
    ["creditcard", "delete", "manage"],
  ]);

  SpecialPowers.clearUserPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);
  SpecialPowers.clearUserPref(AUTOFILL_CREDITCARDS_AVAILABLE_PREF);

  await removeAllRecords();
});

add_task(async function test_saveCreditCard() {
  Services.telemetry.clearEvents();
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

  await SpecialPowers.pushPrefEnv({
    set: [
      [AUTOFILL_CREDITCARDS_AVAILABLE_PREF, true],
      [ENABLED_AUTOFILL_CREDITCARDS_PREF, true],
    ],
  });

  await testDialog(EDIT_CREDIT_CARD_DIALOG_URL, win => {
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-number"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(
      "0" + TEST_CREDIT_CARD_1["cc-exp-month"].toString(),
      {},
      win
    );
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(
      TEST_CREDIT_CARD_1["cc-exp-year"].toString(),
      {},
      win
    );
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-name"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey(TEST_CREDIT_CARD_1["cc-type"], {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    info("saving credit card");
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
  });

  SpecialPowers.clearUserPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);
  SpecialPowers.clearUserPref(AUTOFILL_CREDITCARDS_AVAILABLE_PREF);

  await removeAllRecords();

  await assertTelemetry(undefined, [["creditcard", "add", "manage"]]);
});

add_task(async function test_editCreditCard() {
  Services.telemetry.clearEvents();
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

  await SpecialPowers.pushPrefEnv({
    set: [
      [AUTOFILL_CREDITCARDS_AVAILABLE_PREF, true],
      [ENABLED_AUTOFILL_CREDITCARDS_PREF, true],
    ],
  });

  await saveCreditCard(TEST_CREDIT_CARD_1);

  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "only one credit card is in storage");
  await testDialog(
    EDIT_CREDIT_CARD_DIALOG_URL,
    win => {
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_RIGHT", {}, win);
      EventUtils.synthesizeKey("test", {}, win);
      win.document.querySelector("#save").click();
    },
    {
      record: creditCards[0],
    }
  );

  SpecialPowers.clearUserPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);
  SpecialPowers.clearUserPref(AUTOFILL_CREDITCARDS_AVAILABLE_PREF);

  await removeAllRecords();
  await assertTelemetry(undefined, [
    ["creditcard", "show_entry", "manage"],
    ["creditcard", "edit", "manage"],
  ]);
});

add_task(async function test_histogram() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  await SpecialPowers.pushPrefEnv({
    set: [
      [AUTOFILL_CREDITCARDS_AVAILABLE_PREF, true],
      [ENABLED_AUTOFILL_CREDITCARDS_PREF, true],
    ],
  });

  Services.telemetry.getHistogramById(CC_NUM_USES_HISTOGRAM).clear();
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

  await saveCreditCard(TEST_CREDIT_CARD_1);
  await saveCreditCard(TEST_CREDIT_CARD_2);
  await saveCreditCard(TEST_CREDIT_CARD_5);
  let creditCards = await getCreditCards();
  is(creditCards.length, 3, "3 credit cards in storage");

  assertHistogram(CC_NUM_USES_HISTOGRAM, {
    0: 3,
  });

  await useCreditCard(1);
  assertHistogram(CC_NUM_USES_HISTOGRAM, {
    0: 2,
    1: 1,
  });

  await useCreditCard(2);
  assertHistogram(CC_NUM_USES_HISTOGRAM, {
    0: 1,
    1: 2,
  });

  await useCreditCard(1);
  assertHistogram(CC_NUM_USES_HISTOGRAM, {
    0: 1,
    1: 1,
    2: 1,
  });

  await useCreditCard(2);
  assertHistogram(CC_NUM_USES_HISTOGRAM, {
    0: 1,
    2: 2,
  });

  await useCreditCard(3);
  assertHistogram(CC_NUM_USES_HISTOGRAM, {
    1: 1,
    2: 2,
  });

  SpecialPowers.clearUserPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);
  SpecialPowers.clearUserPref(AUTOFILL_CREDITCARDS_AVAILABLE_PREF);

  await removeAllRecords();

  assertHistogram(CC_NUM_USES_HISTOGRAM, {});
});

add_task(async function test_submit_creditCard_new_with_hidden_ui() {
  const AUTOFILL_CREDITCARDS_HIDE_UI_PREF =
    "extensions.formautofill.creditCards.hideui";

  Services.telemetry.clearEvents();
  Services.telemetry.clearScalars();
  Services.telemetry.getHistogramById(CC_NUM_USES_HISTOGRAM).clear();
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

  await SpecialPowers.pushPrefEnv({
    set: [
      [CREDITCARDS_USED_STATUS_PREF, 0],
      [AUTOFILL_CREDITCARDS_AVAILABLE_PREF, true],
      [ENABLED_AUTOFILL_CREDITCARDS_PREF, true],
      [AUTOFILL_CREDITCARDS_HIDE_UI_PREF, true],
    ],
  });

  await saveCreditCard(TEST_CREDIT_CARD_1);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
      let rejectPopup = () => {
        ok(false, "Popup should not be displayed");
      };
      browser.addEventListener("popupshowing", rejectPopup, true);

      await SimpleTest.promiseFocus(browser);
      await focusAndWaitForFieldsIdentified(browser, "form #cc-number");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);

      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");

      await SpecialPowers.spawn(browser, [], async function() {
        let form = content.document.getElementById("form");
        let name = form.querySelector("#cc-name");

        name.focus();
        name.setUserInput("User 1");

        form.querySelector("#cc-number").setUserInput("5038146897157463");
        form.querySelector("#cc-exp-month").setUserInput("12");
        form.querySelector("#cc-exp-year").setUserInput("2017");
        form.querySelector("#cc-type").value = "mastercard";

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await sleep(1000);
      is(
        PopupNotifications.panel.state,
        "closed",
        "Doorhanger is still hidden"
      );
      browser.removeEventListener("popupshowing", rejectPopup, true);
    }
  );

  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  SpecialPowers.clearUserPref(ENABLED_AUTOFILL_CREDITCARDS_PREF);
  SpecialPowers.clearUserPref(AUTOFILL_CREDITCARDS_AVAILABLE_PREF);
  SpecialPowers.clearUserPref(AUTOFILL_CREDITCARDS_HIDE_UI_PREF);

  assertHistogram(CC_NUM_USES_HISTOGRAM, { 0: 1 });

  let expected_content = [
    ["creditcard", "detected", "cc_form"],
    [
      "creditcard",
      "submitted",
      "cc_form",
      undefined,
      {
        fields_not_auto: "6",
        fields_auto: "0",
        fields_modified: "0",
      },
    ],
  ];
  await assertTelemetry(expected_content, []);
  await removeAllRecords();

  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("content"),
    "formautofill.creditCards.detected_sections_count",
    1,
    "There should be 1 sections detected."
  );
  TelemetryTestUtils.assertScalar(
    TelemetryTestUtils.getProcessScalars("content"),
    "formautofill.creditCards.submitted_sections_count",
    1,
    "There should be 1 section submitted."
  );
});
