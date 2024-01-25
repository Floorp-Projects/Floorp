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
    set: [["extensions.formautofill.addresses.capture.enabled", true]],
  });
});

add_task(async function test_save_doorhanger_tel_invalid() {
  const EXPECTED = [
    {
      "given-name": "John",
      "family-name": "Doe",
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
            "#given-name": "John",
            "#family-name": "Doe",
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

add_task(async function test_save_doorhanger_tel_concatenated() {
  const EXPECTED = [
    {
      "given-name": "John",
      "family-name": "Doe",
      organization: "Mozilla",
      tel: "+15202486621",
    },
  ];

  const MARKUP = `<form id="form">
    <input id="given-name" autocomplete="given-name">
    <input id="family-name" autocomplete="family-name">
    <input id="organization" autocomplete="organization">
    <input id="tel-country-code" autocomplete="tel-country-code">
    <input id="tel-national" autocomplete="tel-national">
    <input type="submit">
   </form>`;

  await expectSavedAddresses([]);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: EMPTY_URL },
    async function (browser) {
      await SpecialPowers.spawn(browser, [MARKUP], doc => {
        content.document.body.innerHTML = doc;
      });

      let onPopupShown = waitForPopupShown();

      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": "John",
          "#family-name": "Doe",
          "#organization": "Mozilla",
          "#tel-country-code": "+1",
          "#tel-national": "5202486621",
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON, 0);
    }
  );

  await expectSavedAddresses(EXPECTED);
  await removeAllRecords();
});
