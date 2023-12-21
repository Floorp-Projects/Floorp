"use strict";

const CC_VALUES = {
  "cc-name": "User",
  "cc-number": "5577000055770004",
  "cc-exp-month": 12,
  "cc-exp-year": 2017,
};

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.creditCards.supported", "on"],
      ["extensions.formautofill.creditCards.enabled", true],
      ["extensions.formautofill.heuristics.captureOnFormRemoval", true],
    ],
  });
  await removeAllRecords();
});

/**
 * Tests if the credit card is captured (cc doorhanger is shown) after a
 * successful xhr or fetch request followed by a form removal and
 * that the stored credit card record has the right values.
 */
add_task(async function test_credit_card_captured_after_form_removal() {
  const onStorageChanged = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      const onPopupShown = waitForPopupShown();
      info("Update identified credit card fields");
      // We don't submit the form
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#cc-name",
          newValues: {
            "#cc-name": CC_VALUES["cc-name"],
            "#cc-number": CC_VALUES["cc-number"],
            "#cc-exp-month": CC_VALUES["cc-exp-month"],
            "#cc-exp-year": CC_VALUES["cc-exp-year"],
          },
        },
        false
      );

      info("Infer a successfull fetch request");
      await SpecialPowers.spawn(browser, [], async () => {
        await content.fetch(
          "https://example.org/browser/browser/extensions/formautofill/test/browser/empty.html"
        );
      });

      info("Infer form removal");
      await SpecialPowers.spawn(browser, [], async function () {
        let form = content.document.getElementById("form");
        form.parentNode.remove(form);
      });

      info("Wait for credit card doorhanger");
      await onPopupShown;

      info("Click Save in credit card doorhanger");
      await clickDoorhangerButton(MAIN_BUTTON);
    }
  );

  info("Wait for the credit card to be added to the storage.");
  await onStorageChanged;

  const storedCreditCards = await getCreditCards();
  let actualCreditCard = storedCreditCards[0];
  actualCreditCard["cc-number"] = await OSKeyStore.decrypt(
    actualCreditCard["cc-number-encrypted"]
  );

  for (let key in CC_VALUES) {
    let expected = CC_VALUES[key];
    let actual = actualCreditCard[key];
    is(expected, actual, `${key} should be equal`);
  }
  await removeAllRecords();
});

/**
 * Tests that the credit card is not captured without a prior fetch or xhr request event
 */
add_task(async function test_credit_card_not_captured_without_prior_fetch() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: CREDITCARD_FORM_URL },
    async function (browser) {
      info("Update identified credit card fields");
      // We don't submit the form
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#cc-name",
          newValues: {
            "#cc-name": CC_VALUES["cc-name"],
            "#cc-number": CC_VALUES["cc-number"],
            "#cc-exp-month": CC_VALUES["cc-exp-month"],
            "#cc-exp-year": CC_VALUES["cc-exp-year"],
          },
        },
        false
      );

      info("Infer form removal");
      await SpecialPowers.spawn(browser, [], async function () {
        let form = content.document.getElementById("form");
        form.parentNode.remove(form);
      });

      info("Ensure that credit card doorhanger is not shown");
      await ensureNoDoorhanger(browser);
    }
  );
});
