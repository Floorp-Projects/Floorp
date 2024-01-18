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

const ADDRESS_FIELD_VALUES = {
  "given-name": "John",
  organization: "Sesame Street",
  "street-address": "123 Sesame Street",
};

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.capture.v2.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
      ["extensions.formautofill.heuristics.captureOnFormRemoval", true],
    ],
  });
  await removeAllRecords();
});

/**
 * Tests if the address is captured (address doorhanger is shown) after a
 * successful xhr or fetch request followed by a form removal and
 * that the stored address record has the right values.
 */
add_task(async function test_address_captured_after_form_removal() {
  const onStorageChanged = waitForStorageChangedEvents("add");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      const onPopupShown = waitForPopupShown();

      info("Update identified address fields");
      // We don't submit the form
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#given-name",
          newValues: {
            "#given-name": ADDRESS_FIELD_VALUES["given-name"],
            "#organization": ADDRESS_FIELD_VALUES.organization,
            "#street-address": ADDRESS_FIELD_VALUES["street-address"],
          },
        },
        false
      );

      info("Infer a successfull fetch request");
      await SpecialPowers.spawn(browser, [], async () => {
        await content.fetch(
          "https://example.org/browser/browser/extensions/formautofill/test/browser/empty.html"
        );
      });

      info("Infer form removal");
      await SpecialPowers.spawn(browser, [], async function () {
        let form = content.document.getElementById("form");
        form.parentNode.remove(form);
      });
      info("Wait for address doorhanger");
      await onPopupShown;

      info("Click Save in address doorhanger");
      await clickDoorhangerButton(MAIN_BUTTON, 0);
    }
  );

  info("Wait for the address to be added to the storage.");
  await onStorageChanged;

  info("Ensure that address record was captured and saved correctly.");
  await expectSavedAddresses([ADDRESS_FIELD_VALUES]);

  await removeAllRecords();
});

/**
 * Tests that the address is not captured without a prior fetch or xhr request event
 */
add_task(async function test_address_not_captured_without_prior_fetch() {
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      info("Update identified address fields");
      // We don't submit the form
      await focusUpdateSubmitForm(
        browser,
        {
          focusSelector: "#given-name",
          newValues: {
            "#given-name": ADDRESS_FIELD_VALUES["given-name"],
            "#organization": ADDRESS_FIELD_VALUES.organization,
            "#street-address": ADDRESS_FIELD_VALUES["street-address"],
          },
        },
        false
      );

      info("Infer form removal");
      await SpecialPowers.spawn(browser, [], async function () {
        let form = content.document.getElementById("form");
        form.parentNode.remove(form);
      });

      info("Ensure that address doorhanger is not shown");
      await ensureNoDoorhanger(browser);
    }
  );
});
