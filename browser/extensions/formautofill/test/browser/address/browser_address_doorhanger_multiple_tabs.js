"use strict";

async function expectedSavedAddresses(expectedCount) {
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

add_task(async function test_address_doorhanger_multiple_tabs() {
  const URL = ADDRESS_FORM_URL;

  expectedSavedAddresses(0);

  const tab1 = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  await showAddressDoorhanger(tab1.linkedBrowser, {
    "#given-name": "John",
    "#organization": "Mozilla",
    "#street-address": "123 Sesame Street",
  });

  const tab2 = await BrowserTestUtils.openNewForegroundTab(gBrowser, URL);
  await showAddressDoorhanger(tab2.linkedBrowser, {
    "#given-name": "Jane",
    "#organization": "Mozilla",
    "#street-address": "123 Sesame Street",
  });

  info(`Save an address in the second tab`);
  await clickDoorhangerButton(MAIN_BUTTON, 0);
  expectedSavedAddresses(1);

  info(`Switch to the first tab and save the address`);
  gBrowser.selectedTab = tab1;
  let anchor = document.getElementById("autofill-address-notification-icon");
  anchor.click();

  await clickDoorhangerButton(MAIN_BUTTON, 0);
  expectedSavedAddresses(2);

  BrowserTestUtils.removeTab(tab1);
  BrowserTestUtils.removeTab(tab2);
});
