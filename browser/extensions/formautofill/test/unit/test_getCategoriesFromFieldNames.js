
"use strict";

Cu.import("resource://formautofill/FormAutofillUtils.jsm");

add_task(async function test_isAddressField_isCreditCardField() {
  const TEST_CASES = {
    "given-name": {
      isAddressField: true,
      isCreditCardField: false,
    },
    "organization": {
      isAddressField: true,
      isCreditCardField: false,
    },
    "address-line2": {
      isAddressField: true,
      isCreditCardField: false,
    },
    "tel": {
      isAddressField: true,
      isCreditCardField: false,
    },
    "email": {
      isAddressField: true,
      isCreditCardField: false,
    },
    "cc-number": {
      isAddressField: false,
      isCreditCardField: true,
    },
    "UNKNOWN": {
      isAddressField: false,
      isCreditCardField: false,
    },
    "": {
      isAddressField: false,
      isCreditCardField: false,
    },
  };

  for (let fieldName of Object.keys(TEST_CASES)) {
    info("Starting testcase: " + fieldName);
    let field = TEST_CASES[fieldName];
    Assert.equal(FormAutofillUtils.isAddressField(fieldName),
                 field.isAddressField,
                 "isAddressField");
    Assert.equal(FormAutofillUtils.isCreditCardField(fieldName),
                 field.isCreditCardField,
                 "isCreditCardField");
  }
});

add_task(async function test_getCategoriesFromFieldNames() {
  const TEST_CASES = [
    {
      fieldNames: ["given-name", "family-name", "name", "tel", "organization"],
      set: ["name", "tel", "organization"],
    },
    {
      fieldNames: ["address-line2", "family-name", "name", "tel", "organization", "email"],
      set: ["address", "name", "tel", "organization", "email"],
    },
    {
      fieldNames: ["address-line2", "family-name", "", "name", "tel", "UNKOWN"],
      set: ["address", "name", "tel"],
    },
    {
      fieldNames: ["tel", "family-name", "", "name", "tel", "UNKOWN"],
      set: ["tel", "name"],
    },
  ];

  for (let tc of TEST_CASES) {
    let categories = FormAutofillUtils.getCategoriesFromFieldNames(tc.fieldNames);
    Assert.deepEqual(Array.from(categories), tc.set);
  }
});
