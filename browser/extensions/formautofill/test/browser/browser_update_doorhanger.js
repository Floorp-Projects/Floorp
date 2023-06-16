"use strict";

/**
 * Note that this testcase is built based on the assumption that subtests are
 * run in order. So if you're going to add a new subtest, please add it in the end.
 */

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.formautofill.addresses.capture.enabled", true]],
  });
});

add_task(async function test_update_address() {
  await setStorage(TEST_ADDRESS_1);
  let addresses = await getAddresses();
  is(addresses.length, 1, "1 address in storage");

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: FORM_URL },
    async function (browser) {
      // Autofill address fields
      await openPopupOn(browser, "form #organization");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await waitForAutofill(browser, "#tel", addresses[0].tel);

      // Update address fields and submit
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#organization",
        newValues: {
          "#organization": "Mozilla",
        },
      });

      await onPopupShown;

      // Choose to update address by doorhanger
      let onUpdated = waitForStorageChangedEvents("update");
      await clickDoorhangerButton(MAIN_BUTTON);
      await onUpdated;
    }
  );

  addresses = await getAddresses();
  is(addresses.length, 1, "Still 1 address in storage");
  is(addresses[0].organization, "Mozilla", "Verify the organization field");
});

add_task(async function test_create_new_address() {
  let addresses = await getAddresses();
  is(addresses.length, 1, "1 address in storage");

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: FORM_URL },
    async function (browser) {
      // Autofill address fields
      await openPopupOn(browser, "form #tel");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await waitForAutofill(browser, "#tel", addresses[0].tel);

      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#tel",
        newValues: {
          "#tel": "+1234567890",
        },
      });

      await onPopupShown;

      // Choose to add address by doorhanger
      let onAdded = waitForStorageChangedEvents("add");
      await clickDoorhangerButton(SECONDARY_BUTTON);
      await onAdded;
    }
  );

  addresses = await getAddresses();
  is(addresses.length, 2, "2 addresses in storage");
  is(addresses[1].tel, "+1234567890", "Verify the tel field");
});

add_task(async function test_create_new_address_merge() {
  let addresses = await getAddresses();
  is(addresses.length, 2, "2 addresses in storage");

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: FORM_URL },
    async function (browser) {
      await openPopupOn(browser, "form #tel");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await waitForAutofill(browser, "#tel", addresses[1].tel);

      // Choose the latest address and revert to the original phone number
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#tel",
        newValues: {
          "#tel": "+16172535702",
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(SECONDARY_BUTTON);
    }
  );

  addresses = await getAddresses();
  is(addresses.length, 2, "Still 2 addresses in storage");
});

add_task(async function test_submit_untouched_fields() {
  let addresses = await getAddresses();
  is(addresses.length, 2, "2 addresses in storage");

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: FORM_URL },
    async function (browser) {
      await openPopupOn(browser, "form #organization");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await waitForAutofill(browser, "#tel", addresses[0].tel);

      let onPopupShown = waitForPopupShown();
      await SpecialPowers.spawn(browser, [], async function () {
        let form = content.document.getElementById("form");
        let tel = form.querySelector("#tel");
        tel.value = "12345"; // ".value" won't change the highlight status.
      });

      await focusUpdateSubmitForm(browser, {
        focusSelector: "#tel",
        newValues: {
          "#organization": "Organization",
        },
      });
      await onPopupShown;

      let onUpdated = waitForStorageChangedEvents("update");
      await clickDoorhangerButton(MAIN_BUTTON);
      await onUpdated;
    }
  );

  addresses = await getAddresses();
  is(addresses.length, 2, "Still 2 addresses in storage");
  is(addresses[0].organization, "Organization", "organization should change");
  is(addresses[0].tel, "+16172535702", "tel should remain unchanged");
});

add_task(async function test_submit_reduced_fields() {
  let addresses = await getAddresses();
  is(addresses.length, 2, "2 addresses in storage");

  let url = BASE_URL + "autocomplete_simple_basic.html";
  await BrowserTestUtils.withNewTab(
    { gBrowser, url },
    async function (browser) {
      await openPopupOn(browser, "form#simple input[name=tel]");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await waitForAutofill(browser, "form #simple_tel", "6172535702");

      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        formSelector: "form#simple",
        focusSelector: "form #simple_tel",
        newValues: {
          "input[name=tel]": "123456789",
        },
      });

      await onPopupShown;

      let onUpdated = waitForStorageChangedEvents("update");
      await clickDoorhangerButton(MAIN_BUTTON);
      await onUpdated;
    }
  );

  addresses = await getAddresses();
  is(addresses.length, 2, "Still 2 addresses in storage");
  is(addresses[0].tel, "123456789", "tel should should be changed");
  is(addresses[0]["postal-code"], "02139", "postal code should be kept");
});
