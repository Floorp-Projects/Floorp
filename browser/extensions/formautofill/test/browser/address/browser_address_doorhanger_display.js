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

add_task(async function test_save_doorhanger_shown_no_profile() {
  await expectSavedAddresses(0);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();

      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": "John",
          "#family-name": "Doe",
          "#organization": "Sesame Street",
          "#street-address": "123 Sesame Street",
          "#tel": "1-345-345-3456",
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON, 0);
    }
  );

  await expectSavedAddresses(1);
  await removeAllRecords();
});

add_task(async function test_save_doorhanger_shown_different_address() {
  await setStorage(TEST_ADDRESS_1);
  await expectSavedAddresses(1);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();

      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": TEST_ADDRESS_2["given-name"],
          "#street-address": TEST_ADDRESS_2["street-address"],
          "#country": TEST_ADDRESS_2.country,
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON, 0);
    }
  );

  await expectSavedAddresses(2);
  await removeAllRecords();
});

add_task(
  async function test_update_doorhanger_shown_change_non_mergeable_given_name() {
    await setStorage(TEST_ADDRESS_1);
    await expectSavedAddresses(1);

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        let onPopupShown = waitForPopupShown();
        await focusUpdateSubmitForm(browser, {
          focusSelector: "#given-name",
          newValues: {
            "#given-name": "John",
            "#family-name": "Doe",
            "#street-address": TEST_ADDRESS_1["street-address"],
            "#country": TEST_ADDRESS_1.country,
            "#email": TEST_ADDRESS_1.email,
          },
        });

        await onPopupShown;
        await clickDoorhangerButton(MAIN_BUTTON, 0);
      }
    );

    await expectSavedAddresses(2);
    await removeAllRecords();
  }
);

add_task(async function test_update_doorhanger_shown_add_email_field() {
  // TEST_ADDRESS_2 doesn't contain email field
  await setStorage(TEST_ADDRESS_2);
  await expectSavedAddresses(1);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      let onPopupShown = waitForPopupShown();
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": TEST_ADDRESS_2["given-name"],
          "#street-address": TEST_ADDRESS_2["street-address"],
          "#country": TEST_ADDRESS_2.country,
          "#email": "test@mozilla.org",
        },
      });

      await onPopupShown;
      await clickDoorhangerButton(MAIN_BUTTON, 0);
    }
  );

  const addresses = await expectSavedAddresses(1);
  is(addresses[0].email, "test@mozilla.org", "Email field is saved");

  await removeAllRecords();
});

add_task(async function test_doorhanger_not_shown_when_autofill_untouched() {
  await setStorage(TEST_ADDRESS_2);
  await expectSavedAddresses(1);

  let onUsed = waitForStorageChangedEvents("notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      await openPopupOn(browser, "form #given-name");
      await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
      await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
      await waitForAutofill(
        browser,
        "#given-name",
        TEST_ADDRESS_2["given-name"]
      );

      await SpecialPowers.spawn(browser, [], async function () {
        let form = content.document.getElementById("form");
        form.querySelector("input[type=submit]").click();
      });

      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
    }
  );
  await onUsed;

  const addresses = await expectSavedAddresses(1);
  is(addresses[0].timesUsed, 1, "timesUsed field set to 1");
  await removeAllRecords();
});

add_task(async function test_doorhanger_not_shown_when_fill_duplicate() {
  await setStorage(TEST_ADDRESS_4);
  await expectSavedAddresses(1);

  let onUsed = waitForStorageChangedEvents("notifyUsed");
  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      await focusUpdateSubmitForm(browser, {
        focusSelector: "#given-name",
        newValues: {
          "#given-name": TEST_ADDRESS_4["given-name"],
          "#family-name": TEST_ADDRESS_4["family-name"],
          "#organization": TEST_ADDRESS_4.organization,
          "#country": TEST_ADDRESS_4.country,
        },
      });

      is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
    }
  );
  await onUsed;

  const addresses = await expectSavedAddresses(1);
  is(
    addresses[0]["given-name"],
    TEST_ADDRESS_4["given-name"],
    "Verify the name field"
  );
  is(addresses[0].timesUsed, 1, "timesUsed field set to 1");
  await removeAllRecords();
});

add_task(
  async function test_doorhanger_not_shown_when_autofill_then_fill_everything_duplicate() {
    await setStorage(TEST_ADDRESS_2, TEST_ADDRESS_3);
    await expectSavedAddresses(2);

    let onUsed = waitForStorageChangedEvents("notifyUsed");
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        await openPopupOn(browser, "form #given-name");
        await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
        await BrowserTestUtils.synthesizeKey("VK_RETURN", {}, browser);
        await waitForAutofill(
          browser,
          "#given-name",
          TEST_ADDRESS_2["given-name"]
        );

        await focusUpdateSubmitForm(browser, {
          focusSelector: "#given-name",
          newValues: {
            // Change number to the second credit card number
            "#given-name": TEST_ADDRESS_3["given-name"],
            "#street-address": TEST_ADDRESS_3["street-address"],
            "#postal-code": TEST_ADDRESS_3["postal-code"],
            "#country": "",
          },
        });

        is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
      }
    );
    await onUsed;

    await expectSavedAddresses(2);
    await removeAllRecords();
  }
);

add_task(
  async function test_doorhanger_shown_when_contain_all_required_fields() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "extensions.formautofill.addresses.capture.requiredFields",
          "street-address,postal-code,address-level1,address-level2",
        ],
      ],
    });

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        let onPopupShown = waitForPopupShown();

        await focusUpdateSubmitForm(browser, {
          focusSelector: "#street-address",
          newValues: {
            "#street-address": "32 Vassar Street\nMIT Room 32-G524",
            "#postal-code": "02139",
            "#address-level2": "Cambridge",
            "#address-level1": "MA",
          },
        });

        await onPopupShown;
        await clickDoorhangerButton(MAIN_BUTTON, 0);
      }
    );

    await expectSavedAddresses(1);
    await SpecialPowers.popPrefEnv();
  }
);

add_task(
  async function test_doorhanger_not_shown_when_contain_required_invalid_fields() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "extensions.formautofill.addresses.capture.requiredFields",
          "street-address,postal-code,address-level1,address-level2",
        ],
      ],
    });

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        await focusUpdateSubmitForm(browser, {
          focusSelector: "#street-address",
          newValues: {
            "#street-address": "32 Vassar Street\nMIT Room 32-G524",
            "#postal-code": "000", // postal-code is invalid
            "#address-level2": "Cambridge",
            "#address-level1": "MA",
          },
        });

        is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
      }
    );
  }
);

add_task(
  async function test_doorhanger_not_shown_when_not_contain_all_required_fields() {
    await SpecialPowers.pushPrefEnv({
      set: [
        [
          "extensions.formautofill.addresses.capture.requiredFields",
          "street-address,postal-code,address-level1,address-level2",
        ],
      ],
    });

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        await focusUpdateSubmitForm(browser, {
          focusSelector: "#street-address",
          newValues: {
            "#given-name": "John",
            "#family-name": "Doe",
            "#postal-code": "02139",
            "#address-level2": "Cambridge",
            "#address-level1": "MA",
          },
        });

        is(PopupNotifications.panel.state, "closed", "Doorhanger is hidden");
      }
    );
  }
);
