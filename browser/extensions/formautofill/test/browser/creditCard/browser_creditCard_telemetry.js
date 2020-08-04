/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

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
    expected_content = expected_content.map(([category, method, object]) => {
      return { category, method, object };
    });

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
    expected_parent = expected_parent.map(([category, method, object]) => {
      return { category, method, object };
    });
    TelemetryTestUtils.assertEvents(
      expected_parent,
      {
        category: "creditcard",
      },
      { process: "parent" }
    );
  }
}

add_task(async function test_popup_opened() {
  Services.telemetry.clearEvents();
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
});

add_task(async function test_submit_creditCard_new() {
  async function test_per_command(command, idx, expectChanged = undefined) {
    await SpecialPowers.pushPrefEnv({
      set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
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
    await removeAllRecords();
  }

  Services.telemetry.clearEvents();
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

  let expected_content = [
    ["creditcard", "detected", "cc_form"],
    [
      "creditcard",
      "submitted",
      "cc_form",
      undefined,
      {
        fields_not_auto: "3",
        fields_auto: "5",
        fields_modified: "5",
      },
    ],
  ];
  await test_per_command(MAIN_BUTTON, undefined, 1);
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
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

  await SpecialPowers.pushPrefEnv({
    set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
  });
  await saveCreditCard(TEST_CREDIT_CARD_1);
  let creditCards = await getCreditCards();
  is(creditCards.length, 1, "1 credit card in storage");

  let osKeyStoreLoginShown = OSKeyStoreTestUtils.waitForOSKeyStoreLogin(true);
  let onUsed = TestUtils.topicObserved(
    "formautofill-storage-changed",
    (subject, data) => data == "notifyUsed"
  );
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function(browser) {
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

        // Wait 1000ms before submission to make sure the input value applied
        await new Promise(resolve => content.setTimeout(resolve, 1000));
        form.querySelector("input[type=submit]").click();
      });

      await sleep(1000);
      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
    }
  );
  await onUsed;

  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
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

  async function test_per_command(command, idx, expectChanged = undefined) {
    await SpecialPowers.pushPrefEnv({
      set: [[CREDITCARDS_USED_STATUS_PREF, 0]],
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

    SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
    await removeAllRecords();
  }
  Services.telemetry.clearEvents();
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

  let expected_content = [
    ["creditcard", "detected", "cc_form"],
    ["creditcard", "popup_shown", "cc_form"],
    ["creditcard", "filled", "cc_form"],
    ["creditcard", "filled_modified", "cc_form"],
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

  await test_per_command(MAIN_BUTTON, undefined, 1);
  await assertTelemetry(expected_content, [
    ["creditcard", "show", "update_doorhanger"],
    ["creditcard", "update", "update_doorhanger"],
  ]);

  await test_per_command(SECONDARY_BUTTON, undefined, 2);
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
  await removeAllRecords();
});

add_task(async function test_saveCreditCard() {
  Services.telemetry.clearEvents();
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

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

  await removeAllRecords();

  await assertTelemetry(undefined, [["creditcard", "add", "manage"]]);
});

add_task(async function test_editCreditCard() {
  Services.telemetry.clearEvents();
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

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
  await removeAllRecords();
  await assertTelemetry(undefined, [
    ["creditcard", "show_entry", "manage"],
    ["creditcard", "edit", "manage"],
  ]);
});
