/* eslint-disable mozilla/no-arbitrary-setTimeout */
"use strict";

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

async function assertTelemetry(expected_content = [], expected_parent = []) {
  let snapshots;

  info(
    `Waiting for ${expected_content.length} content events and ` +
      `${expected_parent.length} parent events`
  );

  await TestUtils.waitForCondition(
    () => {
      snapshots = Services.telemetry.snapshotEvents(
        Ci.nsITelemetry.DATASET_PRERELEASE_CHANNELS,
        false
      );
      return (
        (snapshots.parent?.length ?? 0) >= expected_parent.length &&
        (snapshots.content?.length ?? 0) >= expected_content.length
      );
    },
    "Wait for telemetry to be collected",
    100,
    100
  );

  info(JSON.stringify(snapshots, null, 2));

  if (expected_content.length) {
    expected_content = expected_content.map(([category, method, object]) => {
      return { category, method, object };
    });

    let clear = !expected_parent.length;

    TelemetryTestUtils.assertEvents(
      expected_content,
      {
        category: "creditcard",
      },
      { clear, process: "content" }
    );
  }

  if (expected_parent.length) {
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

add_task(async function test_submit_creditCard_saved() {
  Services.telemetry.clearEvents();
  Services.telemetry.setEventRecordingEnabled("creditcard", true);

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
      await clickDoorhangerButton(MAIN_BUTTON);
      await onChanged;
    }
  );

  is(
    SpecialPowers.getIntPref(CREDITCARDS_USED_STATUS_PREF),
    2,
    "User has seen the doorhanger"
  );
  SpecialPowers.clearUserPref(CREDITCARDS_USED_STATUS_PREF);
  await removeAllRecords();

  await assertTelemetry([
    ["creditcard", "detected", "cc_form"],
    ["creditcard", "submitted", "cc_form"],
  ]);
});
