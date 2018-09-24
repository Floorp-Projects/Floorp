"use strict";

requestLongerTimeout(6);

add_task(async function setup_supportedCountries() {
  await SpecialPowers.pushPrefEnv({set: [
    [SUPPORTED_COUNTRIES_PREF, "US,CA,DE"],
  ]});
});


add_task(async function test_cancelEditAddressDialog() {
  await testDialog(EDIT_ADDRESS_DIALOG_URL, win => {
    win.document.querySelector("#cancel").click();
  });
});

add_task(async function test_cancelEditAddressDialogWithESC() {
  await testDialog(EDIT_ADDRESS_DIALOG_URL, win => {
    EventUtils.synthesizeKey("VK_ESCAPE", {}, win);
  });
});

add_task(async function test_defaultCountry() {
  SpecialPowers.pushPrefEnv({set: [[DEFAULT_REGION_PREF, "CA"]]});
  await testDialog(EDIT_ADDRESS_DIALOG_URL, win => {
    let doc = win.document;
    is(doc.querySelector("#country").value, "CA",
                         "Default country set to Canada");
    doc.querySelector("#cancel").click();
  });
  SpecialPowers.pushPrefEnv({set: [[DEFAULT_REGION_PREF, "DE"]]});
  await testDialog(EDIT_ADDRESS_DIALOG_URL, win => {
    let doc = win.document;
    is(doc.querySelector("#country").value, "DE",
                         "Default country set to Germany");
    doc.querySelector("#cancel").click();
  });
  // Test unsupported country
  SpecialPowers.pushPrefEnv({set: [[DEFAULT_REGION_PREF, "XX"]]});
  await testDialog(EDIT_ADDRESS_DIALOG_URL, win => {
    let doc = win.document;
    is(doc.querySelector("#country").value, "",
                         "Default country set to empty");
    doc.querySelector("#cancel").click();
  });
  SpecialPowers.pushPrefEnv({set: [[DEFAULT_REGION_PREF, "US"]]});
});

add_task(async function test_saveAddress() {
  await testDialog(EDIT_ADDRESS_DIALOG_URL, win => {
    let doc = win.document;
    // Verify labels
    is(doc.querySelector("#address-level1-container > .label-text").textContent, "State",
                         "US address-level1 label should be 'State'");
    is(doc.querySelector("#postal-code-container > .label-text").textContent, "ZIP Code",
                         "US postal-code label should be 'ZIP Code'");
    // Input address info and verify move through form with tab keys
    const keyInputs = [
      "VK_TAB",
      TEST_ADDRESS_1["given-name"],
      "VK_TAB",
      TEST_ADDRESS_1["additional-name"],
      "VK_TAB",
      TEST_ADDRESS_1["family-name"],
      "VK_TAB",
      TEST_ADDRESS_1["street-address"],
      "VK_TAB",
      TEST_ADDRESS_1["address-level2"],
      "VK_TAB",
      TEST_ADDRESS_1["address-level1"],
      "VK_TAB",
      TEST_ADDRESS_1["postal-code"],
      "VK_TAB",
      TEST_ADDRESS_1.organization,
      "VK_TAB",
      // TEST_ADDRESS_1.country, // Country is already US
      "VK_TAB",
      TEST_ADDRESS_1.tel,
      "VK_TAB",
      TEST_ADDRESS_1.email,
      "VK_TAB",
      "VK_TAB",
      "VK_RETURN",
    ];
    keyInputs.forEach(input => EventUtils.synthesizeKey(input, {}, win));
  });
  let addresses = await getAddresses();

  is(addresses.length, 1, "only one address is in storage");
  is(Object.keys(TEST_ADDRESS_1).length, 11, "Sanity check number of properties");
  for (let [fieldName, fieldValue] of Object.entries(TEST_ADDRESS_1)) {
    is(addresses[0][fieldName], fieldValue, "check " + fieldName);
  }
});

add_task(async function test_editAddress() {
  let addresses = await getAddresses();
  await testDialog(EDIT_ADDRESS_DIALOG_URL, win => {
    EventUtils.synthesizeKey("VK_TAB", {}, win);
    EventUtils.synthesizeKey("VK_RIGHT", {}, win);
    EventUtils.synthesizeKey("test", {}, win);
    win.document.querySelector("#save").click();
  }, addresses[0]);
  addresses = await getAddresses();

  is(addresses.length, 1, "only one address is in storage");
  is(addresses[0]["given-name"], TEST_ADDRESS_1["given-name"] + "test", "given-name changed");
  await removeAddresses([addresses[0].guid]);

  addresses = await getAddresses();
  is(addresses.length, 0, "Address storage is empty");
});

add_task(async function test_saveAddressCA() {
  await testDialog(EDIT_ADDRESS_DIALOG_URL, async win => {
    let doc = win.document;
    // Change country to verify labels
    doc.querySelector("#country").focus();
    EventUtils.synthesizeKey("Canada", {}, win);

    await TestUtils.waitForCondition(() => {
      return doc.querySelector("#address-level1-container > .label-text").textContent == "Province";
    }, "Wait for the mutation observer to change the labels");
    is(doc.querySelector("#address-level1-container > .label-text").textContent, "Province",
                         "CA address-level1 label should be 'Province'");
    is(doc.querySelector("#postal-code-container > .label-text").textContent, "Postal Code",
                         "CA postal-code label should be 'Postal Code'");
    is(doc.querySelector("#address-level3-container").style.display, "none",
                         "CA address-level3 should be hidden");

    // Input address info and verify move through form with tab keys
    doc.querySelector("#given-name").focus();
    const keyInputs = [
      TEST_ADDRESS_CA_1["given-name"],
      "VK_TAB",
      TEST_ADDRESS_CA_1["additional-name"],
      "VK_TAB",
      TEST_ADDRESS_CA_1["family-name"],
      "VK_TAB",
      TEST_ADDRESS_CA_1.organization,
      "VK_TAB",
      TEST_ADDRESS_CA_1["street-address"],
      "VK_TAB",
      TEST_ADDRESS_CA_1["address-level2"],
      "VK_TAB",
      TEST_ADDRESS_CA_1["address-level1"],
      "VK_TAB",
      TEST_ADDRESS_CA_1["postal-code"],
      "VK_TAB",
      // TEST_ADDRESS_1.country, // Country is already selected above
      "VK_TAB",
      TEST_ADDRESS_CA_1.tel,
      "VK_TAB",
      TEST_ADDRESS_CA_1.email,
      "VK_TAB",
      "VK_TAB",
      "VK_RETURN",
    ];
    keyInputs.forEach(input => EventUtils.synthesizeKey(input, {}, win));
  });
  let addresses = await getAddresses();
  for (let [fieldName, fieldValue] of Object.entries(TEST_ADDRESS_CA_1)) {
    is(addresses[0][fieldName], fieldValue, "check " + fieldName);
  }
  await removeAllRecords();
});

add_task(async function test_saveAddressDE() {
  await testDialog(EDIT_ADDRESS_DIALOG_URL, async win => {
    let doc = win.document;
    // Change country to verify labels
    doc.querySelector("#country").focus();
    EventUtils.synthesizeKey("Germany", {}, win);
    await TestUtils.waitForCondition(() => {
      return doc.querySelector("#postal-code-container > .label-text").textContent == "Postal Code";
    }, "Wait for the mutation observer to change the labels");
    is(doc.querySelector("#postal-code-container > .label-text").textContent, "Postal Code",
                         "DE postal-code label should be 'Postal Code'");
    is(doc.querySelector("#address-level1-container").style.display, "none",
                         "DE address-level1 should be hidden");
    is(doc.querySelector("#address-level3-container").style.display, "none",
                         "DE address-level3 should be hidden");
    // Input address info and verify move through form with tab keys
    doc.querySelector("#given-name").focus();
    const keyInputs = [
      TEST_ADDRESS_DE_1["given-name"],
      "VK_TAB",
      TEST_ADDRESS_DE_1["additional-name"],
      "VK_TAB",
      TEST_ADDRESS_DE_1["family-name"],
      "VK_TAB",
      TEST_ADDRESS_DE_1.organization,
      "VK_TAB",
      TEST_ADDRESS_DE_1["street-address"],
      "VK_TAB",
      TEST_ADDRESS_DE_1["postal-code"],
      "VK_TAB",
      TEST_ADDRESS_DE_1["address-level2"],
      "VK_TAB",
      // TEST_ADDRESS_1.country, // Country is already selected above
      "VK_TAB",
      TEST_ADDRESS_DE_1.tel,
      "VK_TAB",
      TEST_ADDRESS_DE_1.email,
      "VK_TAB",
      "VK_TAB",
      "VK_RETURN",
    ];
    keyInputs.forEach(input => EventUtils.synthesizeKey(input, {}, win));
  });
  let addresses = await getAddresses();
  for (let [fieldName, fieldValue] of Object.entries(TEST_ADDRESS_DE_1)) {
    is(addresses[0][fieldName], fieldValue, "check " + fieldName);
  }
  await removeAllRecords();
});

add_task(async function test_saveAddressIE() {
  await testDialog(EDIT_ADDRESS_DIALOG_URL, async win => {
    let doc = win.document;
    // Change country to verify labels
    doc.querySelector("#country").focus();
    EventUtils.synthesizeKey("Ireland", {}, win);
    await TestUtils.waitForCondition(() => {
      return doc.querySelector("#postal-code-container > .label-text").textContent == "Eircode";
    }, "Wait for the mutation observer to change the labels");
    is(doc.querySelector("#postal-code-container > .label-text").textContent, "Eircode",
                         "IE postal-code label should be 'Eircode'");
    is(doc.querySelector("#address-level1-container > .label-text").textContent, "County",
                         "IE address-level1 should be 'County'");
    is(doc.querySelector("#address-level3-container > .label-text").textContent, "Townland",
                         "IE address-level3 should be 'Townland'");

    // Input address info and verify move through form with tab keys
    doc.querySelector("#given-name").focus();
    const keyInputs = [
      TEST_ADDRESS_IE_1["given-name"],
      "VK_TAB",
      TEST_ADDRESS_IE_1["additional-name"],
      "VK_TAB",
      TEST_ADDRESS_IE_1["family-name"],
      "VK_TAB",
      TEST_ADDRESS_IE_1.organization,
      "VK_TAB",
      TEST_ADDRESS_IE_1["street-address"],
      "VK_TAB",
      TEST_ADDRESS_IE_1["address-level3"],
      "VK_TAB",
      TEST_ADDRESS_IE_1["address-level2"],
      "VK_TAB",
      TEST_ADDRESS_IE_1["address-level1"],
      "VK_TAB",
      TEST_ADDRESS_IE_1["postal-code"],
      "VK_TAB",
      // TEST_ADDRESS_1.country, // Country is already selected above
      "VK_TAB",
      TEST_ADDRESS_IE_1.tel,
      "VK_TAB",
      TEST_ADDRESS_IE_1.email,
      "VK_TAB",
      "VK_TAB",
      "VK_RETURN",
    ];
    keyInputs.forEach(input => EventUtils.synthesizeKey(input, {}, win));
  });

  let addresses = await getAddresses();
  for (let [fieldName, fieldValue] of Object.entries(TEST_ADDRESS_IE_1)) {
    is(addresses[0][fieldName], fieldValue, "check " + fieldName);
  }
  await removeAllRecords();
});

add_task(async function test_countryFieldLabels() {
  await testDialog(EDIT_ADDRESS_DIALOG_URL, async win => {
    let doc = win.document;
    // Change country to verify labels
    doc.querySelector("#country").focus();

    let mutatableLabels = [
      "postal-code-container",
      "address-level1-container",
      "address-level2-container",
      "address-level3-container",
    ].map(containerID => doc.getElementById(containerID).querySelector(":scope > .label-text"));

    for (let countryOption of doc.querySelector("#country").options) {
      if (countryOption.value == "") {
        info("Skipping the empty option");
        continue;
      }

      // Clear L10N attributes and textContent to not leave leftovers between country tests
      for (let labelEl of mutatableLabels) {
        labelEl.textContent = "";
        delete labelEl.dataset.localization;
      }

      info(`Selecting '${countryOption.label}' (${countryOption.value})`);
      EventUtils.synthesizeKey(countryOption.label, {}, win);

      // Check that the labels were filled
      for (let labelEl of mutatableLabels) {
        await TestUtils.waitForCondition(() => labelEl.textContent,
                                         "Wait for label to be populated by the mutation observer");
        isnot(labelEl.textContent, "",
              "Ensure textContent is non-empty for: " + countryOption.value);
        is(labelEl.dataset.localization, undefined,
           "Ensure data-localization was removed: " + countryOption.value);
      }
    }

    doc.querySelector("#cancel").click();
  });
});

add_task(async function test_combined_name_fields() {
  await testDialog(EDIT_ADDRESS_DIALOG_URL, async win => {
    let doc = win.document;
    let givenNameField = doc.querySelector("#given-name");
    let addtlNameField = doc.querySelector("#additional-name");
    let familyNameField = doc.querySelector("#family-name");

    function getComputedPropertyValue(field, property) {
      return win.getComputedStyle(field).getPropertyValue(property);
    }
    function checkNameComputedPropertiesMatch(field, property, value, checkFn = is) {
      checkFn(getComputedPropertyValue(field, property), value,
              `Check ${field.id}'s ${property} is ${value}`);
    }
    function checkNameFieldBorders(borderColorUnfocused, borderColorFocused) {
      info("checking the perimeter colors");
      checkNameComputedPropertiesMatch(givenNameField, "border-top-color", borderColorFocused);
      checkNameComputedPropertiesMatch(addtlNameField, "border-top-color", borderColorFocused);
      checkNameComputedPropertiesMatch(familyNameField, "border-top-color", borderColorFocused);
      checkNameComputedPropertiesMatch(familyNameField, "border-right-color", borderColorFocused);
      checkNameComputedPropertiesMatch(givenNameField, "border-bottom-color", borderColorFocused);
      checkNameComputedPropertiesMatch(addtlNameField, "border-bottom-color", borderColorFocused);
      checkNameComputedPropertiesMatch(familyNameField, "border-bottom-color", borderColorFocused);
      checkNameComputedPropertiesMatch(givenNameField, "border-left-color", borderColorFocused);

      info("checking the internal borders");
      checkNameComputedPropertiesMatch(givenNameField, "border-right-width", "0px");
      checkNameComputedPropertiesMatch(addtlNameField, "border-left-width", "2px");
      checkNameComputedPropertiesMatch(addtlNameField, "border-left-color", borderColorFocused, isnot);
      checkNameComputedPropertiesMatch(addtlNameField, "border-right-width", "2px");
      checkNameComputedPropertiesMatch(addtlNameField, "border-right-color", borderColorFocused, isnot);
      checkNameComputedPropertiesMatch(familyNameField, "border-left-width", "0px");
    }

    // Set these variables since the test doesn't run from a subdialog and
    // therefore doesn't get the additional common CSS files injected.
    let borderColor = "rgb(0, 255, 0)";
    let borderColorFocused = "rgb(0, 0, 255)";
    doc.body.style.setProperty("--in-content-box-border-color", borderColor);
    doc.body.style.setProperty("--in-content-border-focus", borderColorFocused);

    givenNameField.focus();
    checkNameFieldBorders(borderColor, borderColorFocused);

    addtlNameField.focus();
    checkNameFieldBorders(borderColor, borderColorFocused);

    familyNameField.focus();
    checkNameFieldBorders(borderColor, borderColorFocused);

    info("unfocusing the name fields");
    let cancelButton = doc.querySelector("#cancel");
    cancelButton.focus();
    borderColor = getComputedPropertyValue(givenNameField, "border-top-color");
    isnot(borderColor, borderColorFocused, "Check that the border color is different");
    checkNameFieldBorders(borderColor, borderColor);

    cancelButton.click();
  });
});

add_task(async function test_combined_name_fields_error() {
  await testDialog(EDIT_ADDRESS_DIALOG_URL, async win => {
    let doc = win.document;
    let givenNameField = doc.querySelector("#given-name");
    info("mark the given name field as invalid");
    givenNameField.value = "";
    givenNameField.focus();
    ok(givenNameField.matches(":-moz-ui-invalid"), "Check field is visually invalid");

    let givenNameLabel = doc.querySelector("#given-name-container .label-text");
    // Override pointer-events so that we can use elementFromPoint to know if
    // the label text is visible.
    givenNameLabel.style.pointerEvents = "auto";
    let givenNameLabelRect = givenNameLabel.getBoundingClientRect();
    // Get the center of the label
    let el = doc.elementFromPoint(givenNameLabelRect.left + givenNameLabelRect.width / 2,
                                  givenNameLabelRect.top + givenNameLabelRect.height / 2);

    is(el, givenNameLabel, "Check that the label text is visible in the error state");
    is(win.getComputedStyle(givenNameField).getPropertyValue("border-top-color"),
       "rgba(0, 0, 0, 0)", "Border should be transparent so that only the error outline shows");
    doc.querySelector("#cancel").click();
  });
});
