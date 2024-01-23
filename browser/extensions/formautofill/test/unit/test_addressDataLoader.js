"use strict";

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

var AddressMetaDataLoader, FormAutofillUtils;
add_setup(async () => {
  ({ FormAutofillUtils } = ChromeUtils.importESModule(
    "resource://gre/modules/shared/FormAutofillUtils.sys.mjs"
  ));
  ({ AddressMetaDataLoader } = ChromeUtils.importESModule(
    "resource://gre/modules/shared/AddressMetaDataLoader.sys.mjs"
  ));
});

add_task(async function test_initalState() {
  // addressData should be empty
  Assert.deepEqual(AddressMetaDataLoader.addressData, {});
  // Verify dataLoaded state
  Assert.equal(AddressMetaDataLoader.dataLoaded.country, false);
  Assert.equal(AddressMetaDataLoader.dataLoaded.level1.size, 0);
});

add_task(async function test_loadDataCountry() {
  sinon.spy(AddressMetaDataLoader, "loadAddressMetaData");
  let metadata = FormAutofillUtils.getCountryAddressData("US");
  Assert.ok(AddressMetaDataLoader.addressData, "addressData exists");
  // Verify _dataLoaded state
  Assert.equal(AddressMetaDataLoader.dataLoaded.country, true);
  Assert.equal(AddressMetaDataLoader.dataLoaded.level1.size, 0);
  // _loadAddressMetaData should be called
  sinon.assert.called(AddressMetaDataLoader.loadAddressMetaData);
  // Verify metadata
  Assert.equal(metadata.id, "data/US");
  Assert.ok(
    metadata.alternative_names,
    "US alternative names should be loaded from extension"
  );
  AddressMetaDataLoader.loadAddressMetaData.resetHistory();

  // Load data without country
  let newMetadata = FormAutofillUtils.getCountryAddressData();
  // _loadAddressMetaData should not be called
  sinon.assert.notCalled(AddressMetaDataLoader.loadAddressMetaData);
  Assert.deepEqual(
    metadata,
    newMetadata,
    "metadata should be US if country is not specified"
  );
  AddressMetaDataLoader.loadAddressMetaData.resetHistory();
});

// This test is currently Disable!
// This is because - Loading a non-existent resource could potentially cause a crash
// (See Bug 1859588). To address this issue, We can check for the file's existence
// before attempting to load the script. However, given that we are not using
// state data, just keep the solution simple by disabling the test.
add_task(async function test_loadDataState() {
  sinon.spy(AddressMetaDataLoader, "loadAddressMetaData");
  // Load level 1 data that does not exist
  let undefinedMetadata = FormAutofillUtils.getCountryAddressData("US", "CA");
  // loadAddressMetaData should be called
  sinon.assert.called(AddressMetaDataLoader.loadAddressMetaData);
  Assert.equal(undefinedMetadata, undefined, "metadata should be undefined");
  Assert.ok(
    AddressMetaDataLoader.dataLoaded.level1.has("US"),
    "level 1 state array should be set even there's no valid metadata"
  );
  AddressMetaDataLoader.loadAddressMetaData.resetHistory();

  // Load level 1 data again
  undefinedMetadata = FormAutofillUtils.getCountryAddressData("US", "AS");
  Assert.equal(undefinedMetadata, undefined, "metadata should be undefined");
  // loadAddressMetaData should not be called
  sinon.assert.notCalled(AddressMetaDataLoader.loadAddressMetaData);
}).skip();

SUPPORT_COUNTRIES_TESTCASES.forEach(testcase => {
  add_task(async function test_support_country() {
    info("Starting testcase: Check " + testcase.country + " metadata");
    let metadata = FormAutofillUtils.getCountryAddressData(testcase.country);
    Assert.ok(
      testcase.properties.every(key => metadata[key]),
      "These properties should exist: " + testcase.properties
    );
    // Verify the multi-locale country
    if (metadata.languages && metadata.languages.length > 1) {
      let locales = FormAutofillUtils.getCountryAddressDataWithLocales(
        testcase.country
      );
      Assert.equal(
        metadata.languages.length,
        locales.length,
        "Total supported locales should be matched"
      );
      metadata.languages.forEach((lang, index) => {
        Assert.equal(lang, locales[index].lang, `Should support ${lang}`);
      });
    }
  });
});
