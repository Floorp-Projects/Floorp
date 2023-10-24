/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TESTCASES = [
  {
    description: "@autocomplete - all fields in the same form",
    document: `<form>
                <input id="cc-number" autocomplete="cc-number">
                <input id="cc-name" autocomplete="cc-name">
                <input id="cc-exp" autocomplete="cc-exp">
               </form>`,
    idsToShowPopup: ["cc-number", "cc-name", "cc-exp"],
  },
  {
    description: "without @autocomplete - all fields in the same form",
    document: `<form>
                <input id="cc-number" placeholder="credit card number">
                <input id="cc-name" placeholder="credit card holder name">
                <input id="cc-exp" placeholder="expiration date">
               </form>`,
    idsToShowPopup: ["cc-number", "cc-name", "cc-exp"],
  },
  {
    description: "@autocomplete - each field in its own form",
    document: `<form><input id="cc-number" autocomplete="cc-number"></form>
               <form><input id="cc-name" autocomplete="cc-name"></form>
               <form><input id="cc-exp" autocomplete="cc-exp"></form>`,
    idsToShowPopup: ["cc-number", "cc-name", "cc-exp"],
  },
  {
    description:
      "without @autocomplete - each field in its own form (high-confidence cc-number & cc-name)",
    document: `<form><input id="cc-number" placeholder="credit card number"></form>
               <form><input id="cc-name" placeholder="credit card holder name"></form>
               <form><input id="cc-exp" placeholder="expiration date"></form>`,
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.highConfidenceThreshold",
        "0.9",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.95",
      ],
    ],
    idsToShowPopup: ["cc-number", "cc-name"],
    idsWithNoPopup: ["cc-exp"],
  },
  {
    description:
      "without @autocomplete - each field in its own form (normal-confidence cc-number & cc-name)",
    document: `<form><input id="cc-number" placeholder="credit card number"></form>
               <form><input id="cc-name" placeholder="credit card holder name"></form>
               <form><input id="cc-exp" placeholder="expiration date"></form>`,
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.highConfidenceThreshold",
        "0.9",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.8",
      ],
    ],
    idsWithNoPopup: ["cc-number", "cc-name", "cc-exp"],
  },
  {
    description:
      "with @autocomplete - cc-number/cc-name and another <input> in a form",
    document: `<form>
                 <input id="cc-number" autocomplete="cc-number">
                 <input id="password" type="password">
               </form>
               <form>
                 <input id="cc-name" autocomplete="cc-name">
                 <input id="password" type="password">
               </form>`,
    idsToShowPopup: ["cc-number", "cc-name"],
  },
  {
    description:
      "without @autocomplete - high-confidence cc-number/cc-name and another <input> in a form",
    document: `<form>
                 <input id="cc-number" placeholder="credit card number">
                 <input id="password" type="password">
               </form>
               <form>
                 <input id="cc-name" placeholder="credit card holder name">
                 <input id="password" type="password">
               </form>`,
    idsWithNoPopup: ["cc-number", "cc-name"],
  },
  {
    description:
      "without @autocomplete - high-confidence cc-number/cc-name and another hidden <input> in a form",
    document: `<form>
                 <input id="cc-number" placeholder="credit card number">
                 <input id="token" type="hidden">
               </form>
               <form>
                 <input id="cc-name" placeholder="credit card holder name">
                 <input id="token" type="hidden">
               </form>`,
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.highConfidenceThreshold",
        "0.9",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.95",
      ],
    ],
    idsToShowPopup: ["cc-number", "cc-name"],
  },
];

add_setup(async function () {
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

    await BrowserTestUtils.withNewTab(EMPTY_URL, async function (browser) {
      await SpecialPowers.spawn(browser, [TEST.document], doc => {
        content.document.body.innerHTML = doc;
      });

      await SimpleTest.promiseFocus(browser);

      let ids = TEST.idsToShowPopup ?? [];
      for (const id of ids) {
        await runAndWaitForAutocompletePopupOpen(browser, async () => {
          await focusAndWaitForFieldsIdentified(browser, `#${id}`);
        });
        ok(true, `popup is opened on <input id=${id}>`);
      }

      ids = TEST.idsWithNoPopup ?? [];
      for (const id of ids) {
        await focusAndWaitForFieldsIdentified(browser, `#${id}`);
        await ensureNoAutocompletePopup(browser);
      }
    });

    if (TEST.prefs) {
      await SpecialPowers.popPrefEnv();
    }
  }
});
