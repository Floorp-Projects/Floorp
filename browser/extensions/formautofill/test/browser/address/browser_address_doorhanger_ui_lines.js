"use strict";

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.capture.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
    ],
  });
});

add_task(
  async function test_address_line_displays_normalized_state_in_save_doorhanger() {
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        await showAddressDoorhanger(browser, {
          "#address-level1": "Nova Scotia",
          "#address-level2": "Somerset",
          "#country": "CA",
        });

        const p = getNotification().querySelector(
          `.address-save-update-row-container p:first-child`
        );
        is(p.textContent, "Somerset, NS");

        await clickAddressDoorhangerButton(SECONDARY_BUTTON);
      }
    );
  }
);
