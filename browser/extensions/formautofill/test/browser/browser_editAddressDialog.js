"use strict";

const { FormAutofillUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/shared/FormAutofillUtils.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  Region: "resource://gre/modules/Region.sys.mjs",
});

requestLongerTimeout(6);

add_setup(async function () {
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
    let keypresses = [
      "VK_TAB",
      [
        TEST_ADDRESS_1["given-name"],
        TEST_ADDRESS_1["additional-name"],
        TEST_ADDRESS_1["family-name"],
      ].join(" "),
      "VK_TAB",
      TEST_ADDRESS_1.organization,
      "VK_TAB",
      TEST_ADDRESS_1["street-address"],
      "VK_TAB",
      TEST_ADDRESS_1["address-level2"],
      "VK_TAB",
      TEST_ADDRESS_1["address-level1"],
      "VK_TAB",
      TEST_ADDRESS_1["postal-code"],
      "VK_TAB",
      // TEST_ADDRESS_1.country, // Country is already US
      "VK_TAB",
      TEST_ADDRESS_1.tel,
      "VK_TAB",
      TEST_ADDRESS_1.email,
      "VK_TAB",
    ];
    if (AppConstants.platform != "win") {
      keypresses.push("VK_TAB", "VK_RETURN");
    } else {
      keypresses.push("VK_RETURN");
    }
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
  const name = [
    TEST_ADDRESS_1["given-name"],
    TEST_ADDRESS_1["additional-name"],
    TEST_ADDRESS_1["family-name"],
  ].join(" ");
  is(addresses[0].name, name + "test", "name changed");
  await removeAddresses([addresses[0].guid]);

  addresses = await getAddresses();
  is(addresses.length, 0, "Address storage is empty");
});

add_task(
  async function test_editAddressFrenchCanadianChangedToEnglishRepresentation() {
    let addressClone = Object.assign({}, TEST_ADDRESS_CA_1);
    addressClone["address-level1"] = "Colombie-Britannique";
    await setStorage(addressClone);

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
        win.document.querySelectorAll(":user-invalid").length,
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
    doc.querySelector("#name").focus();
    let keyInputs = [
      [
        TEST_ADDRESS_CA_1["given-name"],
        TEST_ADDRESS_CA_1["additional-name"],
        TEST_ADDRESS_CA_1["family-name"],
      ].join(" "),
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
    ];
    if (AppConstants.platform != "win") {
      keyInputs.push("VK_TAB", "VK_RETURN");
    } else {
      keyInputs.push("VK_RETURN");
    }
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
    doc.querySelector("#name").focus();
    let keyInputs = [
      [
        TEST_ADDRESS_DE_1["given-name"],
        TEST_ADDRESS_DE_1["additional-name"],
        TEST_ADDRESS_DE_1["family-name"],
      ].join(" "),
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
    ];
    if (AppConstants.platform != "win") {
      keyInputs.push("VK_TAB", "VK_RETURN");
    } else {
      keyInputs.push("VK_RETURN");
    }
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
    doc.querySelector("#name").focus();
    let keyInputs = [
      [
        TEST_ADDRESS_IE_1["given-name"],
        TEST_ADDRESS_IE_1["additional-name"],
        TEST_ADDRESS_IE_1["family-name"],
      ].join(" "),
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
    ];
    if (AppConstants.platform != "win") {
      keyInputs.push("VK_TAB", "VK_RETURN");
    } else {
      keyInputs.push("VK_RETURN");
    }
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

    let mutableLabels = [
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

      // Clear L10N textContent to not leave leftovers between country tests
      for (let labelEl of mutableLabels) {
        doc.l10n.setAttributes(labelEl, "");
        labelEl.textContent = "";
      }

      info(`Selecting '${countryOption.label}' (${countryOption.value})`);
      EventUtils.synthesizeKey(countryOption.label, {}, win);

      let l10nResolve;
      let l10nReady = new Promise(resolve => {
        l10nResolve = resolve;
      });
      let verifyL10n = () => {
        if (mutableLabels.every(labelEl => labelEl.textContent)) {
          win.removeEventListener("MozAfterPaint", verifyL10n);
          l10nResolve();
        }
      };
      win.addEventListener("MozAfterPaint", verifyL10n);
      await l10nReady;

      // Check that the labels were filled
      for (let labelEl of mutableLabels) {
        isnot(
          labelEl.textContent,
          "",
          "Ensure textContent is non-empty for: " + countryOption.value
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
          names:
            "Abaco Islands~Acklins~Andros Island~Berry Islands~Bimini~Cat Island~Crooked Island~Eleuthera~Exuma and Cays~Grand Bahama~Harbour Island~Inagua~Long Island~Mayaguana~New Providence~Ragged Island~Rum Cay~San Salvador~Spanish Wells".split(
              "~"
            ),
        },
        US: {
          keys: "AL~AK~AS~AZ~AR~AA~AE~AP~CA~CO~CT~DE~DC~FL~GA~GU~HI~ID~IL~IN~IA~KS~KY~LA~ME~MH~MD~MA~MI~FM~MN~MS~MO~MT~NE~NV~NH~NJ~NM~NY~NC~ND~MP~OH~OK~OR~PW~PA~PR~RI~SC~SD~TN~TX~UT~VT~VI~VA~WA~WV~WI~WY".split(
            "~"
          ),
          names:
            "Alabama~Alaska~American Samoa~Arizona~Arkansas~Armed Forces (AA)~Armed Forces (AE)~Armed Forces (AP)~California~Colorado~Connecticut~Delaware~District of Columbia~Florida~Georgia~Guam~Hawaii~Idaho~Illinois~Indiana~Iowa~Kansas~Kentucky~Louisiana~Maine~Marshall Islands~Maryland~Massachusetts~Michigan~Micronesia~Minnesota~Mississippi~Missouri~Montana~Nebraska~Nevada~New Hampshire~New Jersey~New Mexico~New York~North Carolina~North Dakota~Northern Mariana Islands~Ohio~Oklahoma~Oregon~Palau~Pennsylvania~Puerto Rico~Rhode Island~South Carolina~South Dakota~Tennessee~Texas~Utah~Vermont~Virgin Islands~Virginia~Washington~West Virginia~Wisconsin~Wyoming".split(
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
      // Dispatch a dummy key event so that <select>'s incremental search is cleared.
      EventUtils.synthesizeKey("VK_ACCEPT", {}, win);
    }

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

    // Dispatch a dummy key event so that <select>'s incremental search is cleared.
    EventUtils.synthesizeKey("VK_ACCEPT", {}, win);

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
