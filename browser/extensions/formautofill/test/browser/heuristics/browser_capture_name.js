/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../head.js */

"use strict";

const TESTCASES = [
  {
    description: `cc-number + first name with autocomplete attribute`,
    document: `<form id="form">
                 <input id="cc-number" autocomplete="cc-number">
                 <input id="name" autocomplete="given-name">
                 <input type="submit"/>
               </form>`,
    fillValues: {
      "#cc-number": "4111111111111111",
      "#name": "John Doe",
    },
    expected: "John Doe",
  },
  {
    description: `cc-number + first and last name with autocomplete attribute`,
    document: `<form id="form">
                 <input id="cc-number" autocomplete="cc-number">
                 <input id="given" autocomplete="given-name">
                 <input id="family" autocomplete="family-name">
                 <input type="submit"/>
               </form>`,
    fillValues: {
      "#cc-number": "4111111111111111",
      "#given": "John",
      "#family": "Doe",
    },
    expected: "John Doe",
  },
  {
    description: `cc-number + first and last name without autocomplete attribute`,
    document: `<form id="form">
                <input id="cc-number" autocomplete="cc-number">
                <input id="given" placeholder="First Name">
                <input id="family" placeholder="Last Name">
                 <input type="submit"/>
               </form>`,
    fillValues: {
      "#cc-number": "4111111111111111",
      "#given": "John",
      "#family": "Doe",
    },
    expected: "John Doe",
  },
  {
    description: `name + cc-number with autocomplete attribute`,
    document: `<form id="form">
                 <input id="name" autocomplete="given-name">
                 <input id="cc-number" autocomplete="cc-number">
                 <input type="submit"/>
               </form>`,
    fillValues: {
      "#cc-number": "4111111111111111",
      "#name": "John Doe",
    },
    expected: undefined,
  },
];

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.creditCards.supported", "on"],
      ["extensions.formautofill.creditCards.enabled", true],
    ],
  });
});

add_task(async function test_save_doorhanger_click_save() {
  for (const TEST of TESTCASES) {
    info(`Test ${TEST.description}`);
    let onChanged = waitForStorageChangedEvents("add");
    await BrowserTestUtils.withNewTab(EMPTY_URL, async function (browser) {
      await SpecialPowers.spawn(browser, [TEST.document], doc => {
        // eslint-disable-next-line no-unsanitized/property
        content.document.body.innerHTML = doc;
      });

      await SimpleTest.promiseFocus(browser);

      let onPopupShown = waitForPopupShown();

      await focusUpdateSubmitForm(browser, {
        focusSelector: "#cc-number",
        newValues: TEST.fillValues,
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON);
    });

    await onChanged;

    let creditCards = await getCreditCards();
    is(creditCards.length, 1, "1 credit card in storage");
    dump(`[Dimi]${JSON.stringify(creditCards[0])}\n`);
    is(creditCards[0]["cc-name"], TEST.expected, "Verify the name field");
    await removeAllRecords();
  }
});
