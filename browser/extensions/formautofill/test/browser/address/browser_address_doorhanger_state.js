"use strict";

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
    set: [["extensions.formautofill.addresses.capture.v2.enabled", true]],
  });
});

add_task(async function test_save_doorhanger_state_invalid() {
  const DEFAULT = {
    "given-name": "Test User",
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
    "given-name": "Test User",
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

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        let onPopupShown = waitForPopupShown();

        await focusUpdateSubmitForm(browser, {
          focusSelector: "#given-name",
          newValues: {
            "#given-name": DEFAULT["given-name"],
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
