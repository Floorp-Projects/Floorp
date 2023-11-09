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

// Test different scenarios when we change something in the edit address dorhanger
add_task(async function test_save_edited_fields() {
  await expectSavedAddresses(0);

  const initRecord = {
    "given-name": "John",
    "family-name": "Doe",
    organization: "Mozilla",
    "street-address": "123 Sesame Street",
    tel: "+13453453456",
  };

  const TESTS = [
    {
      description: "adding the email field",
      editedFields: {
        email: "test@mozilla.org",
      },
    },
    {
      description: "changing the given-name field",
      editedFields: {
        name: "Jane",
      },
    },
    {
      description: "appending the street-address field",
      editedFields: {
        "street-address": initRecord["street-address"] + " 4F",
      },
    },
    {
      description: "removing some fields",
      editedFields: {
        name: "",
        tel: "",
      },
    },
    {
      description: "doing all kinds of stuff",
      editedFields: {
        organization: initRecord.organization.toLowerCase(),
        "address-level1": "California",
        tel: "",
        "street-address": initRecord["street-address"] + " Apt.6",
        name: "Jane Doe",
      },
    },
  ];

  for (const TEST of TESTS) {
    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async function (browser) {
        info(`Test ${TEST.description}`);

        const onSavePopupShown = waitForPopupShown();

        await focusUpdateSubmitForm(browser, {
          focusSelector: "#given-name",
          newValues: recordToFormSelector(initRecord),
        });

        await onSavePopupShown;

        const onEditPopupShown = waitForPopupShown();
        await clickAddressDoorhangerButton(EDIT_ADDRESS_BUTTON);
        await onEditPopupShown;
        fillEditDoorhanger(TEST.editedFields);

        await clickAddressDoorhangerButton(MAIN_BUTTON);
      }
    );

    const expectedRecord = normalizeAddressFields({
      ...initRecord,
      ...TEST.editedFields,
    });

    const addresses = await expectSavedAddresses(1);
    for (const [key, value] of Object.entries(expectedRecord)) {
      is(addresses[0][key] ?? "", value, `${key} field is saved`);
    }

    await removeAllRecords();
  }
});

// This test tests edit doorhanger "save" & "cancel" buttons work correctly
// when the edit doorhanger is triggered in an save doorhanger
add_task(async function test_edit_doorhanger_triggered_by_save_doorhanger() {
  for (const CLICKED_BUTTON of [MAIN_BUTTON, SECONDARY_BUTTON]) {
    await expectSavedAddresses(0);

    const initRecord = {
      "given-name": "John",
      "family-name": "Doe",
      organization: "Mozilla",
      "street-address": "123 Sesame Street",
      tel: "+13453453456",
    };

    const editRecord = {
      email: "test@mozilla.org",
    };

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async browser => {
        info(
          `Test clicking ${CLICKED_BUTTON == MAIN_BUTTON ? "save" : "cancel"}`
        );
        const onSavePopupShown = waitForPopupShown();
        await focusUpdateSubmitForm(browser, {
          focusSelector: "#given-name",
          newValues: recordToFormSelector(initRecord),
        });
        await onSavePopupShown;

        const onEditPopupShown = waitForPopupShown();
        await clickAddressDoorhangerButton(EDIT_ADDRESS_BUTTON);
        await onEditPopupShown;
        fillEditDoorhanger(editRecord);

        await clickAddressDoorhangerButton(CLICKED_BUTTON);
      }
    );

    await expectSavedAddresses(CLICKED_BUTTON == MAIN_BUTTON ? 1 : 0);
    await removeAllRecords();
  }
});

// This test tests edit doorhanger "save" & "cancel" buttons work correctly
// when the edit doorhanger is triggered in an update doorhnager
add_task(async function test_edit_doorhanger_triggered_by_update_doorhanger() {
  for (const CLICKED_BUTTON of [MAIN_BUTTON, SECONDARY_BUTTON]) {
    // TEST_ADDRESS_2 doesn't contain email field
    await setStorage(TEST_ADDRESS_2);
    await expectSavedAddresses(1);

    const initRecord = {
      "given-name": TEST_ADDRESS_2["given-name"],
      "street-address": TEST_ADDRESS_2["street-address"],
      country: TEST_ADDRESS_2.country,
      email: "test@mozilla.org",
    };

    const editRecord = {
      email: "test@mozilla.org",
    };

    await BrowserTestUtils.withNewTab(
      { gBrowser, url: ADDRESS_FORM_URL },
      async browser => {
        info(
          `Test clicking ${CLICKED_BUTTON == MAIN_BUTTON ? "save" : "cancel"}`
        );
        const onUpdatePopupShown = waitForPopupShown();
        await focusUpdateSubmitForm(browser, {
          focusSelector: "#given-name",
          newValues: recordToFormSelector(initRecord),
        });
        await onUpdatePopupShown;

        const onEditPopupShown = waitForPopupShown();
        await clickAddressDoorhangerButton(EDIT_ADDRESS_BUTTON);
        await onEditPopupShown;
        fillEditDoorhanger(editRecord);

        await clickAddressDoorhangerButton(CLICKED_BUTTON);
      }
    );

    const addresses = await expectSavedAddresses(1);
    const expectedRecord =
      CLICKED_BUTTON == MAIN_BUTTON
        ? { ...initRecord, ...editRecord }
        : TEST_ADDRESS_2;

    for (const [key, value] of Object.entries(expectedRecord)) {
      is(addresses[0][key] ?? "", value, `${key} field is saved`);
    }

    await removeAllRecords();
  }
});
