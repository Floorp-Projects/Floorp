"use strict";

const VALID_ADDRESS = {
  "given-name": "John",
  "street-address": "32 Vassar Street",
  "address-level1": "California",
  "address-level2": "Cambridge",
  country: "US",
};

const INVALID_ADDRESS = {
  "address-level1": "ZZ", // Invalid state
  organization: "???", // Invalid: only contains punctuation
  email: "john.doe@work@mozilla.org", // Invalid email format
  tel: "2-800-555-1234", // Invalid: wrong country code
  "postal-code": "1234", // Invalid: too short
};

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

/**
 * Submit a form with both valid and invalid fields, we should only
 * save address fields that are valid
 */
add_task(async function test_do_not_save_invalid_fields() {
  let addresses = await expectSavedAddresses(0);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async browser => {
      const onSavePopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": VALID_ADDRESS["given-name"],
          "#family-name": VALID_ADDRESS["family-name"],
          "#street-address": VALID_ADDRESS["street-address"],
          "#address-level1": VALID_ADDRESS["address-level1"],
          "#address-level2": VALID_ADDRESS["address-level2"],

          // Invalid
          "#organization": INVALID_ADDRESS.organization,
          "#email": INVALID_ADDRESS.email,
          "#tel": INVALID_ADDRESS.tel,
          "#postal-code": INVALID_ADDRESS["postal-code"],
        },
      });
      await onSavePopupShown;
      await clickAddressDoorhangerButton(MAIN_BUTTON);
    }
  );

  addresses = await expectSavedAddresses(1);
  for (const [key, value] of Object.entries(VALID_ADDRESS)) {
    Assert.equal(addresses[0][key] ?? "", value, `${key} field is saved`);
  }
  for (const [key, value] of Object.entries(INVALID_ADDRESS)) {
    Assert.notEqual(
      addresses[0][key] ?? "",
      value,
      `${key} field is not saved`
    );
  }
  await removeAllRecords();
});

/**
 * Submit a form with both valid and invalid fields, we should only
 * update address fields that are valid
 */
add_task(async function test_do_not_update_invalid_fields() {
  await setStorage(VALID_ADDRESS);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async browser => {
      const onUpdatePopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": VALID_ADDRESS["given-name"],
          "#family-name": "Doe", // To trigger an update
          "#street-address": VALID_ADDRESS["street-address"],

          // Invalid
          "#address-level1": INVALID_ADDRESS["address-level1"],
          "#organization": INVALID_ADDRESS.organization,
          "#email": INVALID_ADDRESS.email,
          "#tel": INVALID_ADDRESS.tel,
          "#postal-code": INVALID_ADDRESS["postal-code"],
        },
      });
      await onUpdatePopupShown;

      await clickAddressDoorhangerButton(MAIN_BUTTON);
    }
  );

  const addresses = await expectSavedAddresses(1);

  Assert.equal(
    addresses[0]["family-name"],
    "Doe",
    `family-name field is update`
  );
  for (const [key, value] of Object.entries(VALID_ADDRESS)) {
    Assert.equal(addresses[0][key] ?? "", value, `${key} field is saved`);
  }
  for (const [key, value] of Object.entries(INVALID_ADDRESS)) {
    Assert.notEqual(
      addresses[0][key] ?? "",
      value,
      `${key} field is not saved`
    );
  }
  await removeAllRecords();
});

/**
 * it is possibile that existing records contain invalid fields (Users add those
 * fields via preference page). When updating a record with invalid fields, we
 * should still keep those invalid fields in the existing records
 */
add_task(async function test_do_not_remove_invalid_fields_of_exising_address() {
  const STORED_ADDRESS = {
    ...VALID_ADDRESS,
    ...INVALID_ADDRESS,
  };

  // address-level1 in US cannot be invalid, remove it from the storage
  delete STORED_ADDRESS["address-level1"];
  await setStorage(STORED_ADDRESS);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async browser => {
      const onUpdatePopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": VALID_ADDRESS["given-name"],
          "#family-name": "Doe", // To trigger an update
          "#street-address": VALID_ADDRESS["street-address"],
          "#address-level2": VALID_ADDRESS["address-level2"],
        },
      });
      await onUpdatePopupShown;
      await clickAddressDoorhangerButton(MAIN_BUTTON);
    }
  );

  const addresses = await expectSavedAddresses(1);
  Assert.equal(
    addresses[0]["family-name"],
    "Doe",
    `family-name field is update`
  );
  for (const [key, value] of Object.entries(STORED_ADDRESS)) {
    Assert.equal(addresses[0][key] ?? "", value, `${key} field is saved`);
  }
  await removeAllRecords();
});

/**
 * Ensure the edit address doorhanger show the invalid fields of the existing record
 */
add_task(async function test_do_not_show_invalid_fields_in_edit_doorhanger() {
  const STORED_ADDRESS = {
    ...INVALID_ADDRESS,
    ...VALID_ADDRESS, // valid address fields will overwrite invalid address fields
  };
  await setStorage(STORED_ADDRESS);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async browser => {
      const onUpdatePopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": VALID_ADDRESS["given-name"],
          "#family-name": "Doe", // To trigger an update
          "#street-address": VALID_ADDRESS["street-address"],
          "#address-level2": VALID_ADDRESS["address-level2"],
        },
      });
      await onUpdatePopupShown;

      const onEditPopupShown = waitForPopupShown();
      await clickAddressDoorhangerButton(EDIT_ADDRESS_BUTTON);
      await onEditPopupShown;

      await clickAddressDoorhangerButton(MAIN_BUTTON);
    }
  );

  const addresses = await expectSavedAddresses(1);
  Assert.equal(
    addresses[0]["family-name"],
    "Doe",
    `family-name field is update`
  );
  for (const [key, value] of Object.entries(STORED_ADDRESS)) {
    Assert.equal(addresses[0][key] ?? "", value, `${key} field is saved`);
  }
  await removeAllRecords();
});
