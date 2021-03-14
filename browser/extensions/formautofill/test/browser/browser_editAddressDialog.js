"use strict";

const { FormAutofillUtils } = ChromeUtils.import(
  "resource://formautofill/FormAutofillUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  Region: "resource://gre/modules/Region.jsm",
});

requestLongerTimeout(6);

add_task(async function setup_supportedCountries() {
  await SpecialPowers.pushPrefEnv({
    set: [[SUPPORTED_COUNTRIES_PREF, "US,CA,DE"]],
  });
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
  Region._setHomeRegion("CA", false);
  await testDialog(EDIT_ADDRESS_DIALOG_URL, win => {
    let doc = win.document;
    is(
      doc.querySelector("#country").value,
      "CA",
      "Default country set to Canada"
    );
    doc.querySelector("#cancel").click();
  });
  Region._setHomeRegion("DE", false);
  await testDialog(EDIT_ADDRESS_DIALOG_URL, win => {
    let doc = win.document;
    is(
      doc.querySelector("#country").value,
      "DE",
      "Default country set to Germany"
    );
    doc.querySelector("#cancel").click();
  });
  // Test unsupported country
  Region._setHomeRegion("XX", false);
  await testDialog(EDIT_ADDRESS_DIALOG_URL, win => {
    let doc = win.document;
    is(doc.querySelector("#country").value, "", "Default country set to empty");
    doc.querySelector("#cancel").click();
  });
  Region._setHomeRegion("US", false);
});

add_task(async function test_saveAddress() {
  await testDialog(EDIT_ADDRESS_DIALOG_URL, win => {
    let doc = win.document;
    // Verify labels
    is(
      doc.querySelector("#address-level1-container > .label-text").textContent,
      "State",
      "US address-level1 label should be 'State'"
    );
    is(
      doc.querySelector("#postal-code-container > .label-text").textContent,
      "ZIP Code",
      "US postal-code label should be 'ZIP Code'"
    );
    // Input address info and verify move through form with tab keys
    const keypresses = [
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
    keypresses.forEach(keypress => {
      if (
        doc.activeElement.localName == "select" &&
        !keypress.startsWith("VK_")
      ) {
        let field = doc.activeElement;
        while (field.value != keypress) {
          EventUtils.synthesizeKey(keypress[0], {}, win);
        }
      } else {
        EventUtils.synthesizeKey(keypress, {}, win);
      }
    });
  });
  let addresses = await getAddresses();

  is(addresses.length, 1, "only one address is in storage");
  is(
    Object.keys(TEST_ADDRESS_1).length,
    11,
    "Sanity check number of properties"
  );
  for (let [fieldName, fieldValue] of Object.entries(TEST_ADDRESS_1)) {
    is(addresses[0][fieldName], fieldValue, "check " + fieldName);
  }
});

add_task(async function test_editAddress() {
  let addresses = await getAddresses();
  await testDialog(
    EDIT_ADDRESS_DIALOG_URL,
    win => {
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_RIGHT", {}, win);
      EventUtils.synthesizeKey("test", {}, win);

      let stateSelect = win.document.querySelector("#address-level1");
      is(
        stateSelect.selectedOptions[0].value,
        TEST_ADDRESS_1["address-level1"],
        "address-level1 should be selected in the dropdown"
      );

      win.document.querySelector("#save").click();
    },
    {
      record: addresses[0],
    }
  );
  addresses = await getAddresses();

  is(addresses.length, 1, "only one address is in storage");
  is(
    addresses[0]["given-name"],
    TEST_ADDRESS_1["given-name"] + "test",
    "given-name changed"
  );
  await removeAddresses([addresses[0].guid]);

  addresses = await getAddresses();
  is(addresses.length, 0, "Address storage is empty");
});

add_task(
  async function test_editAddressFrenchCanadianChangedToEnglishRepresentation() {
    let addressClone = Object.assign({}, TEST_ADDRESS_CA_1);
    addressClone["address-level1"] = "Colombie-Britannique";
    await saveAddress(addressClone);

    let addresses = await getAddresses();
    await testDialog(
      EDIT_ADDRESS_DIALOG_URL,
      win => {
        let stateSelect = win.document.querySelector("#address-level1");
        is(
          stateSelect.selectedOptions[0].value,
          "BC",
          "address-level1 should have 'BC' selected in the dropdown"
        );

        win.document.querySelector("#save").click();
      },
      {
        record: addresses[0],
      }
    );
    addresses = await getAddresses();

    is(addresses.length, 1, "only one address is in storage");
    is(addresses[0]["address-level1"], "BC", "address-level1 changed");
    await removeAddresses([addresses[0].guid]);

    addresses = await getAddresses();
    is(addresses.length, 0, "Address storage is empty");
  }
);

add_task(async function test_editSparseAddress() {
  let record = { ...TEST_ADDRESS_1 };
  info("delete some usually required properties");
  delete record["street-address"];
  delete record["address-level1"];
  delete record["address-level2"];
  await testDialog(
    EDIT_ADDRESS_DIALOG_URL,
    win => {
      is(
        win.document.querySelectorAll(":-moz-ui-invalid").length,
        0,
        "Check no fields are visually invalid"
      );
      EventUtils.synthesizeKey("VK_TAB", {}, win);
      EventUtils.synthesizeKey("VK_RIGHT", {}, win);
      EventUtils.synthesizeKey("test", {}, win);
      is(
        win.document.querySelector("#save").disabled,
        false,
        "Save button should be enabled after an edit"
      );
      win.document.querySelector("#cancel").click();
    },
    {
      record,
    }
  );
});

add_task(async function test_saveAddressCA() {
  await testDialog(EDIT_ADDRESS_DIALOG_URL, async win => {
    let doc = win.document;
    // Change country to verify labels
    doc.querySelector("#country").focus();
    EventUtils.synthesizeKey("Canada", {}, win);

    await TestUtils.waitForCondition(() => {
      return (
        doc.querySelector("#address-level1-container > .label-text")
          .textContent == "Province"
      );
    }, "Wait for the mutation observer to change the labels");
    is(
      doc.querySelector("#address-level1-container > .label-text").textContent,
      "Province",
      "CA address-level1 label should be 'Province'"
    );
    is(
      doc.querySelector("#postal-code-container > .label-text").textContent,
      "Postal Code",
      "CA postal-code label should be 'Postal Code'"
    );
    is(
      doc.querySelector("#address-level3-container").style.display,
      "none",
      "CA address-level3 should be hidden"
    );

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
      return (
        doc.querySelector("#postal-code-container > .label-text").textContent ==
        "Postal Code"
      );
    }, "Wait for the mutation observer to change the labels");
    is(
      doc.querySelector("#postal-code-container > .label-text").textContent,
      "Postal Code",
      "DE postal-code label should be 'Postal Code'"
    );
    is(
      doc.querySelector("#address-level1-container").style.display,
      "none",
      "DE address-level1 should be hidden"
    );
    is(
      doc.querySelector("#address-level3-container").style.display,
      "none",
      "DE address-level3 should be hidden"
    );
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

/**
 * Test saving an address for a region from regionNames.properties but not in
 * addressReferences.js (libaddressinput).
 */
add_task(async function test_saveAddress_nolibaddressinput() {
  const TEST_ADDRESS = {
    ...TEST_ADDRESS_IE_1,
    ...{
      "address-level3": undefined,
      country: "XG",
    },
  };

  isnot(
    FormAutofillUtils.getCountryAddressData("XG").key,
    "XG",
    "Check that the region we're testing with isn't in libaddressinput"
  );

  await testDialog(EDIT_ADDRESS_DIALOG_URL, async win => {
    let doc = win.document;

    // Change country to verify labels
    doc.querySelector("#country").focus();
    EventUtils.synthesizeKey("Gaza Strip", {}, win);
    await TestUtils.waitForCondition(() => {
      return (
        doc.querySelector("#postal-code-container > .label-text").textContent ==
        "Postal Code"
      );
    }, "Wait for the mutation observer to change the labels");
    is(
      doc.querySelector("#postal-code-container > .label-text").textContent,
      "Postal Code",
      "XG postal-code label should be 'Postal Code'"
    );
    isnot(
      doc.querySelector("#address-level1-container").style.display,
      "none",
      "XG address-level1 should be hidden"
    );
    is(
      doc.querySelector("#address-level2").localName,
      "input",
      "XG address-level2 should be an <input>"
    );
    // Input address info and verify move through form with tab keys
    doc.querySelector("#given-name").focus();
    const keyInputs = [
      TEST_ADDRESS["given-name"],
      "VK_TAB",
      TEST_ADDRESS["additional-name"],
      "VK_TAB",
      TEST_ADDRESS["family-name"],
      "VK_TAB",
      TEST_ADDRESS.organization,
      "VK_TAB",
      TEST_ADDRESS["street-address"],
      "VK_TAB",
      TEST_ADDRESS["address-level2"],
      "VK_TAB",
      TEST_ADDRESS["address-level1"],
      "VK_TAB",
      TEST_ADDRESS["postal-code"],
      "VK_TAB",
      // TEST_ADDRESS_1.country, // Country is already selected above
      "VK_TAB",
      TEST_ADDRESS.tel,
      "VK_TAB",
      TEST_ADDRESS.email,
      "VK_TAB",
      "VK_TAB",
      "VK_RETURN",
    ];
    keyInputs.forEach(input => EventUtils.synthesizeKey(input, {}, win));
  });
  let addresses = await getAddresses();
  for (let [fieldName, fieldValue] of Object.entries(TEST_ADDRESS)) {
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
      return (
        doc.querySelector("#postal-code-container > .label-text").textContent ==
        "Eircode"
      );
    }, "Wait for the mutation observer to change the labels");
    is(
      doc.querySelector("#postal-code-container > .label-text").textContent,
      "Eircode",
      "IE postal-code label should be 'Eircode'"
    );
    is(
      doc.querySelector("#address-level1-container > .label-text").textContent,
      "County",
      "IE address-level1 should be 'County'"
    );
    is(
      doc.querySelector("#address-level3-container > .label-text").textContent,
      "Townland",
      "IE address-level3 should be 'Townland'"
    );

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

add_task(async function test_countryAndStateFieldLabels() {
  await testDialog(EDIT_ADDRESS_DIALOG_URL, async win => {
    let doc = win.document;
    // Change country to verify labels
    doc.querySelector("#country").focus();

    let mutatableLabels = [
      "postal-code-container",
      "address-level1-container",
      "address-level2-container",
      "address-level3-container",
    ].map(containerID =>
      doc.getElementById(containerID).querySelector(":scope > .label-text")
    );

    for (let countryOption of doc.querySelector("#country").options) {
      if (countryOption.value == "") {
        info("Skipping the empty country option");
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
        if (!labelEl.textContent) {
          await TestUtils.waitForCondition(
            () => labelEl.textContent,
            "Wait for label to be populated by the mutation observer",
            10
          );
        }
        isnot(
          labelEl.textContent,
          "",
          "Ensure textContent is non-empty for: " + countryOption.value
        );
        is(
          labelEl.dataset.localization,
          undefined,
          "Ensure data-localization was removed: " + countryOption.value
        );
      }

      let stateOptions = doc.querySelector("#address-level1").options;
      /* eslint-disable max-len */
      let expectedStateOptions = {
        BS: {
          // The Bahamas is an interesting testcase because they have some keys that are full names, and others are replaced with ISO IDs.
          keys: "Abaco~AK~Andros~BY~BI~CI~Crooked Island~Eleuthera~EX~Grand Bahama~HI~IN~LI~MG~N.P.~RI~RC~SS~SW".split(
            "~"
          ),
          names: "Abaco Islands~Acklins~Andros Island~Berry Islands~Bimini~Cat Island~Crooked Island~Eleuthera~Exuma and Cays~Grand Bahama~Harbour Island~Inagua~Long Island~Mayaguana~New Providence~Ragged Island~Rum Cay~San Salvador~Spanish Wells".split(
            "~"
          ),
        },
        US: {
          keys: "AL~AK~AS~AZ~AR~AA~AE~AP~CA~CO~CT~DE~DC~FL~GA~GU~HI~ID~IL~IN~IA~KS~KY~LA~ME~MH~MD~MA~MI~FM~MN~MS~MO~MT~NE~NV~NH~NJ~NM~NY~NC~ND~MP~OH~OK~OR~PW~PA~PR~RI~SC~SD~TN~TX~UT~VT~VI~VA~WA~WV~WI~WY".split(
            "~"
          ),
          names: "Alabama~Alaska~American Samoa~Arizona~Arkansas~Armed Forces (AA)~Armed Forces (AE)~Armed Forces (AP)~California~Colorado~Connecticut~Delaware~District of Columbia~Florida~Georgia~Guam~Hawaii~Idaho~Illinois~Indiana~Iowa~Kansas~Kentucky~Louisiana~Maine~Marshall Islands~Maryland~Massachusetts~Michigan~Micronesia~Minnesota~Mississippi~Missouri~Montana~Nebraska~Nevada~New Hampshire~New Jersey~New Mexico~New York~North Carolina~North Dakota~Northern Mariana Islands~Ohio~Oklahoma~Oregon~Palau~Pennsylvania~Puerto Rico~Rhode Island~South Carolina~South Dakota~Tennessee~Texas~Utah~Vermont~Virgin Islands~Virginia~Washington~West Virginia~Wisconsin~Wyoming".split(
            "~"
          ),
        },
      };
      /* eslint-enable max-len */

      if (expectedStateOptions[countryOption.value]) {
        let { keys, names } = expectedStateOptions[countryOption.value];
        is(
          stateOptions.length,
          keys.length + 1,
          "stateOptions should list all options plus a blank entry"
        );
        is(stateOptions[0].value, "", "First State option should be blank");
        for (let i = 1; i < stateOptions.length; i++) {
          is(
            stateOptions[i].value,
            keys[i - 1],
            "Each State should be listed in alphabetical name order (key)"
          );
          is(
            stateOptions[i].text,
            names[i - 1],
            "Each State should be listed in alphabetical name order (name)"
          );
        }
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
    function checkNameComputedPropertiesMatch(
      field,
      property,
      value,
      checkFn = is
    ) {
      checkFn(
        getComputedPropertyValue(field, property),
        value,
        `Check ${field.id}'s ${property} is ${value}`
      );
    }
    function checkNameFieldBorders(borderColorUnfocused, borderColorFocused) {
      info("checking the perimeter colors");
      checkNameComputedPropertiesMatch(
        givenNameField,
        "border-top-color",
        borderColorFocused
      );
      checkNameComputedPropertiesMatch(
        addtlNameField,
        "border-top-color",
        borderColorFocused
      );
      checkNameComputedPropertiesMatch(
        familyNameField,
        "border-top-color",
        borderColorFocused
      );
      checkNameComputedPropertiesMatch(
        familyNameField,
        "border-right-color",
        borderColorFocused
      );
      checkNameComputedPropertiesMatch(
        givenNameField,
        "border-bottom-color",
        borderColorFocused
      );
      checkNameComputedPropertiesMatch(
        addtlNameField,
        "border-bottom-color",
        borderColorFocused
      );
      checkNameComputedPropertiesMatch(
        familyNameField,
        "border-bottom-color",
        borderColorFocused
      );
      checkNameComputedPropertiesMatch(
        givenNameField,
        "border-left-color",
        borderColorFocused
      );

      info("checking the internal borders");
      checkNameComputedPropertiesMatch(
        givenNameField,
        "border-right-width",
        "0px"
      );
      checkNameComputedPropertiesMatch(
        addtlNameField,
        "border-left-width",
        "2px"
      );
      checkNameComputedPropertiesMatch(
        addtlNameField,
        "border-left-color",
        borderColorFocused,
        isnot
      );
      checkNameComputedPropertiesMatch(
        addtlNameField,
        "border-right-width",
        "2px"
      );
      checkNameComputedPropertiesMatch(
        addtlNameField,
        "border-right-color",
        borderColorFocused,
        isnot
      );
      checkNameComputedPropertiesMatch(
        familyNameField,
        "border-left-width",
        "0px"
      );
    }

    // Set these variables since the test doesn't run from a subdialog and
    // therefore doesn't get the additional common CSS files injected.
    let borderColor = "rgb(0, 255, 0)";
    let borderColorFocused = "rgb(0, 0, 255)";
    doc.body.style.setProperty("--in-content-box-border-color", borderColor);
    doc.body.style.setProperty(
      "--in-content-border-active",
      borderColorFocused
    );

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
    isnot(
      borderColor,
      borderColorFocused,
      "Check that the border color is different"
    );
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
    ok(
      givenNameField.matches(":-moz-ui-invalid"),
      "Check field is visually invalid"
    );

    let givenNameLabel = doc.querySelector("#given-name-container .label-text");
    // Override pointer-events so that we can use elementFromPoint to know if
    // the label text is visible.
    givenNameLabel.style.pointerEvents = "auto";
    let givenNameLabelRect = givenNameLabel.getBoundingClientRect();
    // Get the center of the label
    let el = doc.elementFromPoint(
      givenNameLabelRect.left + givenNameLabelRect.width / 2,
      givenNameLabelRect.top + givenNameLabelRect.height / 2
    );

    is(
      el,
      givenNameLabel,
      "Check that the label text is visible in the error state"
    );
    is(
      win.getComputedStyle(givenNameField).getPropertyValue("border-top-color"),
      "rgba(0, 0, 0, 0)",
      "Border should be transparent so that only the error outline shows"
    );
    doc.querySelector("#cancel").click();
  });
});

add_task(async function test_hiddenFieldNotSaved() {
  await testDialog(EDIT_ADDRESS_DIALOG_URL, win => {
    let doc = win.document;
    doc.querySelector("#address-level2").focus();
    EventUtils.synthesizeKey(TEST_ADDRESS_1["address-level2"], {}, win);
    doc.querySelector("#address-level1").focus();
    EventUtils.synthesizeKey(TEST_ADDRESS_1["address-level1"], {}, win);
    doc.querySelector("#country").focus();
    EventUtils.synthesizeKey("Germany", {}, win);
    doc.querySelector("#save").focus();
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
  });
  let addresses = await getAddresses();
  is(addresses[0].country, "DE", "check country");
  is(
    addresses[0]["address-level2"],
    TEST_ADDRESS_1["address-level2"],
    "check address-level2"
  );
  is(
    addresses[0]["address-level1"],
    undefined,
    "address-level1 should not be saved"
  );

  await removeAllRecords();
});

add_task(async function test_hiddenFieldRemovedWhenCountryChanged() {
  let addresses = await getAddresses();
  ok(!addresses.length, "no addresses at start of test");
  await testDialog(EDIT_ADDRESS_DIALOG_URL, win => {
    let doc = win.document;
    doc.querySelector("#address-level2").focus();
    EventUtils.synthesizeKey(TEST_ADDRESS_1["address-level2"], {}, win);
    doc.querySelector("#address-level1").focus();
    while (
      doc.querySelector("#address-level1").value !=
      TEST_ADDRESS_1["address-level1"]
    ) {
      EventUtils.synthesizeKey(TEST_ADDRESS_1["address-level1"][0], {}, win);
    }
    doc.querySelector("#save").focus();
    EventUtils.synthesizeKey("VK_RETURN", {}, win);
  });
  addresses = await getAddresses();
  is(addresses[0].country, "US", "check country");
  is(
    addresses[0]["address-level2"],
    TEST_ADDRESS_1["address-level2"],
    "check address-level2"
  );
  is(
    addresses[0]["address-level1"],
    TEST_ADDRESS_1["address-level1"],
    "check address-level1"
  );

  await testDialog(
    EDIT_ADDRESS_DIALOG_URL,
    win => {
      let doc = win.document;
      doc.querySelector("#country").focus();
      EventUtils.synthesizeKey("Germany", {}, win);
      win.document.querySelector("#save").click();
    },
    {
      record: addresses[0],
    }
  );
  addresses = await getAddresses();

  is(addresses.length, 1, "only one address is in storage");
  is(
    addresses[0]["address-level2"],
    TEST_ADDRESS_1["address-level2"],
    "check address-level2"
  );
  is(
    addresses[0]["address-level1"],
    undefined,
    "address-level1 should be removed"
  );
  is(addresses[0].country, "DE", "country changed");
  await removeAllRecords();
});

add_task(async function test_countrySpecificFieldsGetRequiredness() {
  Region._setHomeRegion("RO", false);
  await testDialog(EDIT_ADDRESS_DIALOG_URL, async win => {
    let doc = win.document;
    is(
      doc.querySelector("#country").value,
      "RO",
      "Default country set to Romania"
    );
    let provinceField = doc.getElementById("address-level1");
    ok(
      !provinceField.required,
      "address-level1 should not be marked as required"
    );
    ok(provinceField.disabled, "address-level1 should be marked as disabled");
    is(
      provinceField.parentNode.style.display,
      "none",
      "address-level1 is hidden for Romania"
    );

    doc.querySelector("#country").focus();
    EventUtils.synthesizeKey("United States", {}, win);

    await TestUtils.waitForCondition(
      () => {
        provinceField = doc.getElementById("address-level1");
        return provinceField.parentNode.style.display != "none";
      },
      "Wait for address-level1 to become visible",
      10
    );

    ok(provinceField.required, "address-level1 should be marked as required");
    ok(
      !provinceField.disabled,
      "address-level1 should not be marked as disabled"
    );

    doc.querySelector("#country").focus();
    EventUtils.synthesizeKey("Romania", {}, win);

    await TestUtils.waitForCondition(
      () => {
        provinceField = doc.getElementById("address-level1");
        return provinceField.parentNode.style.display == "none";
      },
      "Wait for address-level1 to become hidden",
      10
    );

    ok(
      provinceField.required,
      "address-level1 will still be marked as required"
    );
    ok(provinceField.disabled, "address-level1 should be marked as disabled");

    doc.querySelector("#cancel").click();
  });
});
