/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TESTCASES = [
  {
    description: "Form containing credit card autocomplete attributes.",
    document: `<form>
                <input id="cc-number" autocomplete="cc-number">
                <input id="cc-name" autocomplete="cc-name">
                <input id="cc-exp-month" autocomplete="cc-exp-month">
                <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    idsToShowPopup: ["cc-number", "cc-name", "cc-exp-month", "cc-exp-year"],
  },
  {
    description: "Form containing credit card without autocomplete attributes.",
    document: `<form>
                <input id="cc-number" name="credit card number">
                <input id="cc-name" name="credit card holder name">
                <input id="cc-exp-month" name="expiration month">
                <input id="cc-exp-year" name="expiration year">
               </form>`,
    idsToShowPopup: ["cc-number", "cc-name", "cc-exp-month", "cc-exp-year"],
  },
  {
    description: "one input field cc-number with autocomplete",
    document: `<form>
                <input id="cc-number" autocomplete="cc-number">
               </form>`,
    idsToShowPopup: ["cc-number"],
  },
  {
    description:
      "one input field cc-number without autocomplete (high confidence)",
    document: `<form>
                <input id="cc-number" name="credit card number">
               </form>`,
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.numberOnly.confidenceThreshold",
        "0.9",
      ],
      ["extensions.formautofill.creditCards.heuristics.testConfidence", "0.95"],
    ],
    idsToShowPopup: ["cc-number"],
  },
  {
    description:
      "one input field cc-number without autocomplete (low confidence)",
    document: `<form>
                <input id="cc-number" name="credit card number">
               </form>`,
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.numberOnly.confidenceThreshold",
        "0.9",
      ],
      ["extensions.formautofill.creditCards.heuristics.testConfidence", "0.8"],
    ],
    idsWithNoPopup: ["cc-number"],
  },
  {
    description: "two input fields cc-number with autocomplete",
    document: `<form>
                <input id="cc-number" name="credit card number" autocomplete="cc-number">
                <input id="password" type="password">
               </form>`,
    idsToShowPopup: ["cc-number"],
  },
  {
    description: "two input fields cc-number without autocomplete",
    document: `<form>
                <input id="cc-number" name="credit card number">
                <input id="password" type="password">
               </form>`,
    idsWithNoPopup: ["cc-number"],
  },
  {
    description: "two input fields cc-number and a hidden input field",
    document: `<form>
                <input id="cc-number" name="credit card number">
                <input id="token" type="hidden">
               </form>`,
    prefs: [
      ["extensions.formautofill.creditCards.heuristics.testConfidence", "0.95"],
    ],
    idsToShowPopup: ["cc-number"],
  },
];

add_setup(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.creditCards.supported", "on"],
      ["extensions.formautofill.creditCards.enabled", true],
    ],
  });

  await setStorage(TEST_CREDIT_CARD_1);
});

add_task(async function test_heuristics() {
  for (const TEST of TESTCASES) {
    info(`Test ${TEST.description}`);
    if (TEST.prefs) {
      await SpecialPowers.pushPrefEnv({ set: TEST.prefs });
    }

    await BrowserTestUtils.withNewTab(EMPTY_URL, async function(browser) {
      await SpecialPowers.spawn(browser, [TEST.document], doc => {
        // eslint-disable-next-line no-unsanitized/property
        content.document.body.innerHTML = doc;
      });

      await SimpleTest.promiseFocus(browser);

      let ids = TEST.idsToShowPopup ?? [];
      for (const id of ids) {
        await runAndWaitForAutocompletePopupOpen(browser, async () => {
          await focusAndWaitForFieldsIdentified(browser, `#${id}`);
        });
        ok(true, `popup is opened on <input id=${id}`);
      }

      ids = TEST.idsWithNoPopup ?? [];
      for (const id of ids) {
        await focusAndWaitForFieldsIdentified(browser, `#${id}`);
        await ensureNoAutocompletePopup(browser);
        ok(true, `popup is not opened on <input id=${id}`);
      }
    });

    if (TEST.prefs) {
      await SpecialPowers.popPrefEnv();
    }
  }
});
