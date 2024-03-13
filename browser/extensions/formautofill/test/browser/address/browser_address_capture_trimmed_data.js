"use strict";

const DEFAULT_TEST_DOC = `
<form id="form">
  <input id="street-address" autocomplete="street-address">
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
<input type="submit"/>
</form>`;

const TESTCASES = [
  {
    description: "Save address with trimmed address data",
    document: DEFAULT_TEST_DOC,
    targetElementId: "street-address",
    formValue: {
      "#street-address": "331 E. Evelyn Avenue  ",
      "#country": "US",
      "#email": "  ",
      "#tel": "1-650-903-0800",
    },
    expected: [
      {
        "street-address": "331 E. Evelyn Avenue",
        country: "US",
        email: "",
        tel: "+16509030800",
      },
    ],
  },
];

async function expectSavedAddresses(expectedAddresses) {
  const addresses = await getAddresses();
  is(
    addresses.length,
    expectedAddresses.length,
    `${addresses.length} address in the storage`
  );

  for (let i = 0; i < expectedAddresses.length; i++) {
    for (const [key, value] of Object.entries(expectedAddresses[i])) {
      is(addresses[i][key] ?? "", value, `field ${key} should be equal`);
    }
  }
  return addresses;
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
      ["extensions.formautofill.addresses.capture.enabled", true],
    ],
  });
});

add_task(async function test_address_capture_trimmed_data() {
  for (const TEST of TESTCASES) {
    info(`Test ${TEST.description}`);

    const onChanged = waitForStorageChangedEvents("add");
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

    await expectSavedAddresses(TEST.expected);
    await removeAllRecords();
  }
});
