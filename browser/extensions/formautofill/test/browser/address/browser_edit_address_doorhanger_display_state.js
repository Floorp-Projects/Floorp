"use strict";
requestLongerTimeout(2);

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.formautofill.addresses.capture.v2.enabled", true]],
  });
});

add_task(async function test_edit_doorhanger_display_state() {
  const DEFAULT = {
    "given-name": "Test User",
    organization: "Mozilla",
    "street-address": "123 Sesame Street",
    country: "US",
  };

  const TEST_CASES = [
    {
      filled: { "address-level1": "floridaa" }, // typo
      expected: { label: "" },
    },
    {
      filled: { "address-level1": "AB" }, // non-exist region code
      expected: { label: "" },
    },
    {
      filled: { "address-level1": "CA" },
      expected: { label: "CA" },
    },
    {
      filled: { "address-level1": "fl" },
      expected: { label: "FL" },
    },
    {
      filled: { "address-level1": "New York" },
      expected: { label: "NY" },
    },
    {
      filled: { "address-level1": "Washington" },
      expected: { label: "WA" },
    },
  ];

  for (const TEST of TEST_CASES) {
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        const onSavePopupShown = waitForPopupShown();
        await focusUpdateSubmitForm(browser, {
          focusSelector: "#given-name",
          newValues: {
            "#given-name": DEFAULT["given-name"],
            "#organization": DEFAULT.organization,
            "#street-address": DEFAULT["street-address"],
            "#address-level1": TEST.filled["address-level1"],
          },
        });
        await onSavePopupShown;

        const onEditPopupShown = waitForPopupShown();
        await clickAddressDoorhangerButton(EDIT_ADDRESS_BUTTON);
        await onEditPopupShown;

        const notification = getNotification();
        const id = AddressEditDoorhanger.getInputId("address-level1");
        const element = notification.querySelector(`#${id}`);

        is(
          element.label,
          TEST.expected.label,
          "Edit address doorhanger shows the expected address-level1 select option"
        );
      }
    );
  }
});
