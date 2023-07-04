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

add_task(async function test_save_doorhanger_tel_invalid() {
  const EXPECTED = [
    {
      "given-name": "Test User",
      organization: "Mozilla",
      "street-address": "123 Sesame Street",
      tel: "",
    },
  ];

  const TEST_CASES = [
    "1234", // length is too short
    "1234567890123456", // length is too long
    "12345###!!", // contains invalid characters
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
            "#given-name": "Test User",
            "#organization": "Mozilla",
            "#street-address": "123 Sesame Street",
            "#tel": TEST,
          },
        });

        await onPopupShown;
        await clickDoorhangerButton(MAIN_BUTTON, 0);
      }
    );

    await expectSavedAddresses(EXPECTED);
    await removeAllRecords();
  }
});
