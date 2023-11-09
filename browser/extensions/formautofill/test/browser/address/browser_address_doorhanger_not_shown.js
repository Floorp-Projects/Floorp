"use strict";

const DEFAULT_TEST_DOC = `<form id="form">
  <input id="street-addr" autocomplete="street-address">
  <select id="address-level1" autocomplete="address-level1">
    <option value=""></option>
    <option value="AL">Alabama</option>
    <option value="AK">Alaska</option>
    <option value="AP">Armed Forces Pacific</option>

    <option value="ca">california</option>
    <option value="AR">US-Arkansas</option>
    <option value="US-CA">California</option>
    <option value="CA">California</option>
    <option value="US-AZ">US_Arizona</option>
    <option value="Ariz">Arizonac</option>
  </select>
  <input id="city" autocomplete="address-level2">
  <input id="country" autocomplete="country">
  <input id="email" autocomplete="email">
  <input id="tel" autocomplete="tel">
  <input id="cc-name" autocomplete="cc-name">
  <input id="cc-number" autocomplete="cc-number">
  <input id="cc-exp-month" autocomplete="cc-exp-month">
  <input id="cc-exp-year" autocomplete="cc-exp-year">
  <select id="cc-type">
    <option value="">Select</option>
    <option value="visa">Visa</option>
    <option value="mastercard">Master Card</option>
    <option value="amex">American Express</option>
  </select>
  <input id="submit" type="submit">
</form>`;
const TARGET_ELEMENT_ID = "street-addr";

const TESTCASES = [
  {
    description:
      "Should not trigger address saving if the number of fields is less than 3",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "#street-addr": "331 E. Evelyn Avenue",
      "#tel": "1-650-903-0800",
    },
  },
  {
    description: "Should not trigger the address save doorhanger when pref off",
    document: DEFAULT_TEST_DOC,
    targetElementId: TARGET_ELEMENT_ID,
    formValue: {
      "#street-addr": "331 E. Evelyn Avenue",
      "#email": "test@mozilla.org",
      "#tel": "1-650-903-0800",
    },
    prefs: [["extensions.formautofill.addresses.capture.v2.enabled", false]],
  },
];

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.capture.v2.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
    ],
  });
});

add_task(async function test_save_doorhanger_not_shown() {
  for (const TEST of TESTCASES) {
    info(`Test ${TEST.description}`);
    if (TEST.prefs) {
      await SpecialPowers.pushPrefEnv({
        set: TEST.prefs,
      });
    }

    await BrowserTestUtils.withNewTab(EMPTY_URL, async function (browser) {
      await SpecialPowers.spawn(browser, [TEST.document], doc => {
        content.document.body.innerHTML = doc;
      });

      await SimpleTest.promiseFocus(browser);

      await focusUpdateSubmitForm(browser, {
        focusSelector: `#${TEST.targetElementId}`,
        newValues: TEST.formValue,
      });

      await ensureNoDoorhanger(browser);
    });

    if (TEST.prefs) {
      await SpecialPowers.popPrefEnv();
    }
  }
});
