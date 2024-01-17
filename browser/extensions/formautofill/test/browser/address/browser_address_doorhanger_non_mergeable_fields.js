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
      ["extensions.formautofill.addresses.capture.v2.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
    ],
  });
});

/**
 * Submit a form with both mergeable and non-mergeable fields, we should only
 * update address fields that are mergeable
 */
add_task(async function test_do_not_update_non_mergeable_fields() {
  const TEST_ADDRESS = {
    "address-level1": "New York",
    "address-level2": "New York City",
    "street-address": "32 Vassar Street 3F",
    email: "john.doe@mozilla.com",
    "postal-code": "12345-1234",
    organization: "MOZILLA",
    country: "US",
  };
  await setStorage(TEST_ADDRESS);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async browser => {
      const onUpdatePopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": "John",
          "#family-name": "Doe",
          "#address-level1": "NY", // NY == New York
          "#address-level2": "New York", // non-mergeable
          "#street-address": "32 Vassar Street", // non-mergeable
          "#email": "", // non-mergeable
          "#postal-code": "12345", // non-mergeable
          "#organization": TEST_ADDRESS.organization.toLowerCase(),
          "#country": "US",
        },
      });
      await onUpdatePopupShown;

      await clickAddressDoorhangerButton(MAIN_BUTTON);
    }
  );

  const addresses = await expectSavedAddresses(1);

  Assert.equal(addresses[0]["given-name"], "John", `given-name is added`);
  Assert.equal(addresses[0]["family-name"], "Doe", `family-name is added`);
  Assert.equal(
    addresses[0]["street-address"],
    TEST_ADDRESS["street-address"],
    `street-address is not updated`
  );
  Assert.equal(
    addresses[0]["address-level1"],
    TEST_ADDRESS["address-level1"],
    `address-level1 is not updated`
  );
  Assert.equal(
    addresses[0]["address-level2"],
    TEST_ADDRESS["address-level2"],
    `address-level2 is not updated`
  );
  Assert.equal(addresses[0].email, TEST_ADDRESS.email, `email is not update`);
  Assert.equal(
    addresses[0]["postal-code"],
    "12345-1234",
    `postal-code not is update`
  );
  Assert.equal(
    addresses[0].organization,
    TEST_ADDRESS.organization.toLowerCase(),
    `organization is update`
  );

  await removeAllRecords();
});
