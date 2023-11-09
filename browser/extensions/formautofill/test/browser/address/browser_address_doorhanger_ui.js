"use strict";

const { FormAutofill } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofill.sys.mjs"
);

async function expectSavedAddresses(expectedCount) {
  const addresses = await getAddresses();
  is(
    addresses.length,
    expectedCount,
    `${addresses.length} address in the storage`
  );
  return addresses;
}

function verifyDoorhangerContent(saved, removed = {}) {
  const rows = [
    ...getNotification().querySelectorAll(`.address-save-update-row-container`),
  ];

  let texts = rows.reduce((acc, cur) => acc + cur.textContent, "");
  for (const text of Object.values(saved)) {
    ok(texts.includes(text), `Show ${text} in the doorhanger`);
    texts = texts.replace(text, "");
  }
  for (const text of Object.values(removed)) {
    ok(texts.includes(text), `Show ${text} in the doorhanger (removed)`);
    texts = texts.replace(text, "");
  }
  is(texts.trim(), "", `Doorhanger shows all the submitted data`);
}

function checkVisibility(element) {
  return element.checkVisibility({
    checkOpacity: true,
    checkVisibilityCSS: true,
  });
}

function recordToFormSelector(record) {
  let obj = {};
  for (const [key, value] of Object.entries(record)) {
    obj[`#${key}`] = value;
  }
  return obj;
}

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.addresses.capture.v2.enabled", true],
      ["extensions.formautofill.addresses.supported", "on"],
    ],
  });
});

// Save address doorhanger should show description when users has no saved address
add_task(async function test_save_doorhanger_show_description() {
  await expectSavedAddresses(0);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      await showAddressDoorhanger(browser);

      const header = AutofillDoorhanger.header(getNotification());
      is(checkVisibility(header), true, "Should always show header");

      const description = AutofillDoorhanger.description(getNotification());
      is(
        checkVisibility(description),
        true,
        "Should show description when this is the first address saved"
      );
    }
  );
});

// Save address doorhanger should not show description when users has at least one saved address
add_task(async function test_save_doorhanger_hide_description() {
  await setStorage(TEST_ADDRESS_1);
  await expectSavedAddresses(1);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      await showAddressDoorhanger(browser);

      const header = AutofillDoorhanger.header(getNotification());
      is(checkVisibility(header), true, "Should always show header");

      const description = AutofillDoorhanger.description(getNotification());
      is(
        checkVisibility(description),
        false,
        "Should not show description when there is at least one saved address"
      );
    }
  );

  await removeAllRecords();
});

// Test open edit address popup and then click "learn more" button
add_task(async function test_click_learn_more_button_in_edit_doorhanger() {
  await expectSavedAddresses(0);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      await showAddressDoorhanger(browser);

      let tabOpenPromise = BrowserTestUtils.waitForNewTab(gBrowser, url =>
        url.endsWith(AddressSaveDoorhanger.learnMoreURL)
      );
      await clickAddressDoorhangerButton(
        ADDRESS_MENU_BUTTON,
        ADDRESS_MENU_LEARN_MORE
      );
      const tab = await tabOpenPromise;
      gBrowser.removeTab(tab);
    }
  );
});

add_task(async function test_click_address_setting_button_in_edit_doorhanger() {
  await expectSavedAddresses(0);

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      await showAddressDoorhanger(browser);

      let tabOpenPromise = BrowserTestUtils.waitForNewTab(
        gBrowser,
        `about:preferences#${AddressSaveDoorhanger.preferenceURL}`
      );
      await clickAddressDoorhangerButton(
        ADDRESS_MENU_BUTTON,
        ADDRESS_MENU_PREFENCE
      );
      const tab = await tabOpenPromise;
      gBrowser.removeTab(tab);
    }
  );
});

add_task(async function test_address_display_in_save_doorhanger() {
  await expectSavedAddresses(0);

  const TESTS = [
    {
      description: "Test submit a form without email and tel fields",
      form: {
        "#family-name": "Doe",
        "#organization": "Mozilla",
        "#street-address": "123 Sesame Street",
      },
      expectedSectionCount: 1,
    },
    {
      description: "Test submit a form with email field",
      form: {
        "#given-name": "John",
        "#organization": "Mozilla",
        "#street-address": "123 Sesame Street",
        "#email": "test@mozilla.org",
      },
      expectedSectionCount: 2,
    },
    {
      description: "Test submit a form with tel field",
      form: {
        "#given-name": "John",
        "#family-name": "Doe",
        "#organization": "Mozilla",
        "#street-address": "123 Sesame Street",
        "#tel": "+13453453456",
      },
      expectedSectionCount: 2,
    },
  ];

  for (const TEST of TESTS) {
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        info(TEST.description);
        await showAddressDoorhanger(browser, TEST.form);

        is(
          getNotification().querySelectorAll(
            `.address-save-update-row-container`
          ).length,
          TEST.expectedSectionCount,
          `Should have ${TEST.expectedSectionCount} address section`
        );

        // When the form has no country field, doorhanger shows the default region
        verifyDoorhangerContent({
          ...TEST.form,
          country: FormAutofill.DEFAULT_REGION,
        });

        await clickAddressDoorhangerButton(SECONDARY_BUTTON);
      }
    );
  }

  await removeAllRecords();
});

add_task(async function test_show_added_text_in_update_doorhanger() {
  await setStorage(TEST_ADDRESS_2);
  await expectSavedAddresses(1);

  const form = {
    ...TEST_ADDRESS_2,

    email: "test@mozilla.org", // Add email field
    "given-name": TEST_ADDRESS_2["given-name"] + " Doe", // Append
    "street-address": TEST_ADDRESS_2["street-address"] + " 4F", // Append
  };

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      await showAddressDoorhanger(browser, recordToFormSelector(form));

      // When the form has no country field, doorhanger shows the default region
      verifyDoorhangerContent({
        ...form,
        country: FormAutofill.DEFAULT_REGION,
      });

      await clickAddressDoorhangerButton(SECONDARY_BUTTON);
    }
  );

  await removeAllRecords();
});

add_task(async function test_show_removed_text_in_update_doorhanger() {
  const SAVED_ADDRESS = {
    ...TEST_ADDRESS_2,
    organization: "Mozilla",
  };
  await setStorage(SAVED_ADDRESS);
  await expectSavedAddresses(1);

  // We will ask whether users would like to update "Mozilla" to "mozilla"
  const form = {
    ...SAVED_ADDRESS,

    organization: SAVED_ADDRESS.organization.toLowerCase(),
  };

  await BrowserTestUtils.withNewTab(
    { gBrowser, url: ADDRESS_FORM_URL },
    async function (browser) {
      await showAddressDoorhanger(browser, recordToFormSelector(form));

      // When the form has no country field, doorhanger shows the default region
      verifyDoorhangerContent(
        { ...form, country: FormAutofill.DEFAULT_REGION },
        { organization: SAVED_ADDRESS.organization }
      );

      await clickAddressDoorhangerButton(SECONDARY_BUTTON);
    }
  );

  await removeAllRecords();
});
