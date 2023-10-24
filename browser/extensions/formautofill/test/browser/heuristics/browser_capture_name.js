/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* import-globals-from ../head.js */

"use strict";

const TESTCASES = [
  {
    description: `cc fields + first name with autocomplete attribute`,
    document: `<form id="form">
                 <input id="cc-number" autocomplete="cc-number">
                 <input id="cc-exp" autocomplete="cc-exp">
                 <input id="name" autocomplete="given-name">
                 <input type="submit"/>
               </form>`,
    fillValues: {
      "#cc-number": "4111111111111111",
      "#cc-exp": "12/24",
      "#name": "John Doe",
    },
    expected: undefined,
  },
  {
    description: `cc fields + first name without autocomplete attribute`,
    document: `<form id="form">
                 <input id="cc-number" autocomplete="cc-number">
                 <input id="cc-exp" autocomplete="cc-exp">
                 <input id="name" placeholder="given-name">
                 <input type="submit"/>
               </form>`,
    fillValues: {
      "#cc-number": "4111111111111111",
      "#cc-exp": "12/24",
      "#name": "John Doe",
    },
    expected: "John Doe",
  },
  {
    description: `cc fields + first and last name with autocomplete attribute`,
    document: `<form id="form">
                 <input id="cc-number" autocomplete="cc-number">
                 <input id="cc-exp" autocomplete="cc-exp">
                 <input id="given" autocomplete="given-name">
                 <input id="family" autocomplete="family-name">
                 <input type="submit"/>
               </form>`,
    fillValues: {
      "#cc-number": "4111111111111111",
      "#cc-exp": "12/24",
      "#given": "John",
      "#family": "Doe",
    },
    expected: undefined,
  },
  {
    description: `cc fields + first and last name without autocomplete attribute`,
    document: `<form id="form">
                 <input id="cc-number" autocomplete="cc-number">
                 <input id="cc-exp" autocomplete="cc-exp">
                 <input id="given" placeholder="given-name">
                 <input id="family" placeholder="family-name">
                 <input type="submit"/>
               </form>`,
    fillValues: {
      "#cc-number": "4111111111111111",
      "#cc-exp": "12/24",
      "#given": "John",
      "#family": "Doe",
    },
    expected: "John Doe",
  },
  {
    description: `cc fields + cc-name + first and last name`,
    document: `<form id="form">
                 <input id="cc-number" autocomplete="cc-number">
                 <input id="cc-name" autocomplete="cc-name">
                 <input id="cc-exp" autocomplete="cc-exp">
                 <input id="given" placeholder="given-name">
                 <input id="family" placeholder="family-name">
                 <input type="submit"/>
               </form>`,
    fillValues: {
      "#cc-number": "4111111111111111",
      "#cc-name": "Jane Poe",
      "#cc-exp": "12/24",
      "#given": "John",
      "#family": "Doe",
    },
    expected: "Jane Poe",
  },
  {
    description: `first and last name + cc fields`,
    document: `<form id="form">
                 <input id="given" placeholder="given-name">
                 <input id="family" placeholder="family-name">
                 <input id="cc-number" autocomplete="cc-number">
                 <input id="cc-exp" autocomplete="cc-exp">
                 <input type="submit"/>
               </form>`,
    fillValues: {
      "#cc-number": "4111111111111111",
      "#cc-exp": "12/24",
      "#given": "John",
      "#family": "Doe",
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
    is(creditCards[0]["cc-name"], TEST.expected, "Verify the name field");
    await removeAllRecords();
  }
});
