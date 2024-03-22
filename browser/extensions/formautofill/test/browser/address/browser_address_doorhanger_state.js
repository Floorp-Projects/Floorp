"use strict";
requestLongerTimeout(3);

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

add_task(async function test_save_doorhanger_state_invalid() {
  const DEFAULT = {
    "given-name": "John",
    "family-name": "Doe",
    organization: "Mozilla",
    "street-address": "123 Sesame Street",
    country: "US",
  };

  const TEST_CASES = [
    {
      filled: { "address-level1": "floridaa" }, // typo
      expected: { "address-level1": "" },
    },
    {
      filled: { "address-level1": "AB" }, // non-exist region code
      expected: { "address-level1": "" },
    },
  ];

  for (const TEST of TEST_CASES) {
    await expectSavedAddresses([]);

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        let onPopupShown = waitForPopupShown();

        await focusUpdateSubmitForm(browser, {
          focusSelector: "#given-name",
          newValues: {
            "#given-name": DEFAULT["given-name"],
            "#family-name": DEFAULT["family-name"],
            "#organization": DEFAULT.organization,
            "#street-address": DEFAULT["street-address"],
            "#address-level1": TEST.filled["address-level1"],
          },
        });

        await onPopupShown;
        await clickDoorhangerButton(MAIN_BUTTON, 0);
      }
    );

    await expectSavedAddresses([Object.assign(DEFAULT, TEST.expected)]);
    await removeAllRecords();
  }
});

add_task(async function test_save_doorhanger_state_valid() {
  const DEFAULT = {
    "given-name": "John",
    "family-name": "Doe",
    organization: "Mozilla",
    "street-address": "123 Sesame Street",
    country: "US",
  };

  const TEST_CASES = [
    {
      filled: { "address-level1": "ca" },
      expected: { "address-level1": "ca" },
    },
    {
      filled: { "address-level1": "CA" },
      expected: { "address-level1": "CA" },
    },
    {
      filled: { "address-level1": "CA-BC" },
      expected: { "address-level1": "CA-BC" },
    },
    {
      filled: { "address-level1": "california" },
      expected: { "address-level1": "california" },
    },
    {
      filled: { "address-level1": "California" },
      expected: { "address-level1": "California" },
    },
  ];

  for (const TEST of TEST_CASES) {
    await expectSavedAddresses([]);

    const onChanged = waitForStorageChangedEvents("add");
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        let onPopupShown = waitForPopupShown();

        await focusUpdateSubmitForm(browser, {
          focusSelector: "#given-name",
          newValues: {
            "#given-name": DEFAULT["given-name"],
            "#family-name": DEFAULT["family-name"],
            "#organization": DEFAULT.organization,
            "#street-address": DEFAULT["street-address"],
            "#address-level1": TEST.filled["address-level1"],
          },
        });

        await onPopupShown;
        await clickDoorhangerButton(MAIN_BUTTON, 0);
      }
    );
    await onChanged;

    await expectSavedAddresses([Object.assign(DEFAULT, TEST.expected)]);
    await removeAllRecords();
  }
});

add_task(async function test_save_doorhanger_state_is_select() {
  const DEFAULT_TEST_DOC = `
    <form id="form">
      <input id="name" autocomplete="name">
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
      <input id="submit" type="submit">
    </form>`;

  const TEST_CASES = [
    {
      description: "Save state with regular select option",
      filled: { "address-level1": "CA" },
      expected: { "address-level1": "CA" },
    },
    {
      description: "Save state with lowercase value",
      filled: { "address-level1": "ca" },
      expected: { "address-level1": "CA" },
    },
    {
      description: "Save state with a country code prefixed to the label",
      filled: { "address-level1": "AR" },
      expected: { "address-level1": "AR" },
    },
    {
      description: "Save state with a country code prefixed to the value",
      filled: { "address-level1": "US-CA" },
      expected: { "address-level1": "CA" },
    },
    {
      description:
        "Save state with a country code prefixed to the value and label",
      filled: { "address-level1": "US-AZ" },
      expected: { "address-level1": "AZ" },
    },
    {
      description: "Should not save when failed to abbreviate the value",
      filled: { "address-level1": "Ariz" },
      expected: { "address-level1": "" },
    },
    {
      description: "Should not save select with multiple selections",
      filled: { "address-level1": ["AL", "AK", "AP"] },
      expected: { "address-level1": "" },
    },
  ];

  for (const TEST of TEST_CASES) {
    const onChanged = waitForStorageChangedEvents("add");
    await BrowserTestUtils.withNewTab(EMPTY_URL, async function (browser) {
      info(`Test ${TEST.description}`);

      await SpecialPowers.spawn(browser, [DEFAULT_TEST_DOC], doc => {
        content.document.body.innerHTML = doc;
      });

      await SimpleTest.promiseFocus(browser);

      const onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#name",
        newValues: {
          "#name": "John Doe",
          "#street-address": "Main Street",
          "#country": "US",
          "#address-level1": TEST.filled["address-level1"],
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON, 0);
    });
    await onChanged;

    await expectSavedAddresses([TEST.expected]);
    await removeAllRecords();
  }
});
