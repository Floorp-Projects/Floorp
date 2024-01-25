/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

async function expectSavedAddresses(expectedCount) {
  const addresses = await getAddresses();
  is(
    addresses.length,
    expectedCount,
    `${addresses.length} address in the storage`
  );
  return addresses;
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.capture.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
    ],
  });
});

add_task(async function test_save_doorhanger_show_confirmation() {
  await expectSavedAddresses(0);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      // Show the save doorhanger
      const onSavePopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": "Test User",
          "#organization": "Sesame Street",
          "#street-address": "123 Sesame Street",
          "#tel": "1-345-345-3456",
        },
      });
      await onSavePopupShown;

      // click the main button and expect seeing the confirmation
      const hintShownAndVerified = verifyConfirmationHint(browser, false);
      await clickDoorhangerButton(MAIN_BUTTON, 0);

      info("waiting for verifyConfirmationHint");
      await hintShownAndVerified;
      info("waiting for verifyConfirmationHint <<");
    }
  );

  await expectSavedAddresses(1);
  await removeAllRecords();
});

add_task(async function test_update_doorhanger_show_confirmation() {
  await setStorage(TEST_ADDRESS_3);
  await expectSavedAddresses(1);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      // Show the update doorhanger
      const onUpdatePopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": TEST_ADDRESS_3["given-name"],
          "#street-address": `${TEST_ADDRESS_3["street-address"]} 4F`,
          "#postal-code": TEST_ADDRESS_3["postal-code"],
        },
      });
      await onUpdatePopupShown;

      // click the main button and expect seeing the confirmation
      const hintShownAndVerified = verifyConfirmationHint(browser, false);
      await clickDoorhangerButton(MAIN_BUTTON, 0);

      info("waiting for verifyConfirmationHint");
      await hintShownAndVerified;
    }
  );

  await expectSavedAddresses(1);
  await removeAllRecords();
});

add_task(async function test_edit_doorhanger_show_confirmation() {
  await expectSavedAddresses(0);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      // Show the save doorhanger
      const onSavePopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": "Test User",
          "#organization": "Sesame Street",
          "#street-address": "123 Sesame Street",
          "#tel": "1-345-345-3456",
        },
      });
      await onSavePopupShown;

      // Show the edit doorhanger
      const onEditPopupShown = waitForPopupShown();
      await clickAddressDoorhangerButton(EDIT_ADDRESS_BUTTON);
      await onEditPopupShown;

      // click the main button and expect seeing the confirmation
      const hintShownAndVerified = verifyConfirmationHint(browser, false);
      await clickDoorhangerButton(MAIN_BUTTON, 0);

      info("waiting for verifyConfirmationHint");
      await hintShownAndVerified;
    }
  );

  await expectSavedAddresses(1);
  await removeAllRecords();
});
