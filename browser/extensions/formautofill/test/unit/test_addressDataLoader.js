"use strict";

Cu.import("resource://formautofill/FormAutofillUtils.jsm");

const SUPPORT_COUNTRIES_TESTCASES = [
  {
    country: "US",
    properties: ["languages", "alternative_names", "sub_keys", "sub_names"],
  },
  {
    country: "CA",
    properties: ["languages", "name", "sub_keys", "sub_names"],
  },
  {
    country: "DE",
    properties: ["name"],
  },
];

add_task(async function test_initalState() {
  // addressData should not exist
  Assert.equal(AddressDataLoader._addressData, undefined);
  // Verify _dataLoaded state
  Assert.equal(AddressDataLoader._dataLoaded.country, false);
  Assert.equal(AddressDataLoader._dataLoaded.level1.size, 0);
});

add_task(async function test_loadDataState() {
  sinon.spy(AddressDataLoader, "_loadScripts");
  let metadata = FormAutofillUtils.getCountryAddressData("US");
  Assert.ok(AddressDataLoader._addressData, "addressData exists");
  // Verify _dataLoaded state
  Assert.equal(AddressDataLoader._dataLoaded.country, true);
  Assert.equal(AddressDataLoader._dataLoaded.level1.size, 0);
  // _loadScripts should be called
  sinon.assert.called(AddressDataLoader._loadScripts);
  // Verify metadata
  Assert.equal(metadata.id, "data/US");
  Assert.ok(metadata.alternative_names,
            "US alternative names should be loaded from extension");
  AddressDataLoader._loadScripts.reset();

  // Load data without country
  let newMetadata = FormAutofillUtils.getCountryAddressData();
  // _loadScripts should not be called
  sinon.assert.notCalled(AddressDataLoader._loadScripts);
  Assert.deepEqual(metadata, newMetadata, "metadata should be US if country is not specified");
  AddressDataLoader._loadScripts.reset();

  // Load level 1 data that does not exist
  let undefinedMetadata = FormAutofillUtils.getCountryAddressData("US", "CA");
  // _loadScripts should be called
  sinon.assert.called(AddressDataLoader._loadScripts);
  Assert.equal(undefinedMetadata, undefined, "metadata should be undefined");
  Assert.ok(AddressDataLoader._dataLoaded.level1.has("US"),
               "level 1 state array should be set even there's no valid metadata");
  AddressDataLoader._loadScripts.reset();

  // Load level 1 data again
  undefinedMetadata = FormAutofillUtils.getCountryAddressData("US", "AS");
  Assert.equal(undefinedMetadata, undefined, "metadata should be undefined");
  // _loadScripts should not be called
  sinon.assert.notCalled(AddressDataLoader._loadScripts);
});

SUPPORT_COUNTRIES_TESTCASES.forEach(testcase => {
  add_task(async function test_support_country() {
    info("Starting testcase: Check " + testcase.country + " metadata");
    let metadata = FormAutofillUtils.getCountryAddressData(testcase.country);
    Assert.ok(testcase.properties.every(key => metadata[key]),
              "These properties should exist: " + testcase.properties);
    // Verify the multi-locale country
    if (metadata.languages && metadata.languages.length > 1) {
      let locales = FormAutofillUtils.getCountryAddressDataWithLocales(testcase.country);
      Assert.equal(metadata.languages.length, locales.length, "Total supported locales should be matched");
      metadata.languages.forEach((lang, index) => {
        Assert.equal(lang, locales[index].lang, `Should support ${lang}`);
      });
    }
  });
});
