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
  async function test_per_command(command, idx, expectChanged) {
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
        if (expectChanged) {
          await onChanged;
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
    ["creditcard", "submitted", "cc_form"],
  ];
  await test_per_command(MAIN_BUTTON, undefined, true);
  await assertTelemetry(expected_content, [
    ["creditcard", "show", "capture_doorhanger"],
    ["creditcard", "save", "capture_doorhanger"],
  ]);

  await test_per_command(SECONDARY_BUTTON, undefined, false);
  await assertTelemetry(expected_content, [
    ["creditcard", "show", "capture_doorhanger"],
    ["creditcard", "cancel", "capture_doorhanger"],
  ]);

  await test_per_command(MENU_BUTTON, 0, false);
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
      ["creditcard", "submitted", "cc_form"],
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

  async function test_per_command(command, idx, expectChanged) {
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
        if (expectChanged) {
          await onChanged;
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
    ["creditcard", "submitted", "cc_form"],
  ];

  await test_per_command(MAIN_BUTTON, undefined, true);
  await assertTelemetry(expected_content, [
    ["creditcard", "show", "update_doorhanger"],
    ["creditcard", "update", "update_doorhanger"],
  ]);

  await test_per_command(SECONDARY_BUTTON, undefined, true);
  await assertTelemetry(expected_content, [
    ["creditcard", "show", "update_doorhanger"],
    ["creditcard", "save", "update_doorhanger"],
  ]);
});
