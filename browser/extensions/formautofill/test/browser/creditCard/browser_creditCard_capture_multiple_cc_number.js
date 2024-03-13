"use strict";

const TESTCASES = [
  {
    description: "Trigger credit card saving using multiple cc-number fields",
    document: `<form id="form">
      <input id="cc-name" autocomplete="cc-name">
      <input id="cc-number1" autocomplete="cc-number" maxlength="4">
      <input id="cc-number2" autocomplete="cc-number" maxlength="4">
      <input id="cc-number3" autocomplete="cc-number" maxlength="4">
      <input id="cc-number4" autocomplete="cc-number" maxlength="4">
      <input id="cc-exp-month" autocomplete="cc-exp-month">
      <input id="cc-exp-year" autocomplete="cc-exp-year">
      <input id="submit" type="submit">
    </form>`,
    targetElementId: "cc-number1",
    formValue: {
      "#cc-name": "John Doe",
      "#cc-number1": "3714",
      "#cc-number2": "4963",
      "#cc-number3": "5398",
      "#cc-number4": "431",
      "#cc-exp-month": 12,
      "#cc-exp-year": 2000,
    },
    expected: [
      {
        "cc-name": "John Doe",
        "cc-number": "371449635398431",
        "cc-exp-month": 12,
        "cc-exp-year": 2000,
        "cc-type": "amex",
      },
    ],
  },
];

async function expectSavedCreditCards(expectedCreditCards) {
  const creditcards = await getCreditCards();
  is(
    creditcards.length,
    expectedCreditCards.length,
    `${creditcards.length} credit card in the storage`
  );

  for (let i = 0; i < expectedCreditCards.length; i++) {
    for (const [key, value] of Object.entries(expectedCreditCards[i])) {
      if (key == "cc-number") {
        creditcards[i]["cc-number"] = await OSKeyStore.decrypt(
          creditcards[i]["cc-number-encrypted"]
        );
      }
      is(creditcards[i][key] ?? "", value, `field ${key} should be equal`);
    }
  }
  return creditcards;
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.creditCards.supported", "on"],
      ["extensions.formautofill.creditCards.enabled", true],
    ],
  });
});

add_task(async function test_capture_multiple_cc_number() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  for (const TEST of TESTCASES) {
    info(`Test ${TEST.description}`);

    let onChanged = waitForStorageChangedEvents("add");
    await BrowserTestUtils.withNewTab(EMPTY_URL, async function (browser) {
      await SpecialPowers.spawn(browser, [TEST.document], doc => {
        content.document.body.innerHTML = doc;
      });

      await SimpleTest.promiseFocus(browser);

      const onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: `#${TEST.targetElementId}`,
        newValues: TEST.formValue,
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON, 0);
    });
    await onChanged;

    await expectSavedCreditCards(TEST.expected);
    await removeAllRecords();
  }
});
