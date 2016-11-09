/*
 * Test for populating field values in Form Autofill Parent.
 */

/* global FormAutofillParent */

"use strict";

importAutofillModule("FormAutofillParent.jsm");

do_get_profile();

const TEST_FIELDS = [
  {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "organization"},
  {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
  {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2"},
  {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level1"},
  {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "postal-code"},
  {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country"},
  {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},
  {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},
];

const TEST_GUID = "test-guid";

const TEST_PROFILE = {
  guid: TEST_GUID,
  organization: "World Wide Web Consortium",
  streetAddress: "32 Vassar Street\nMIT Room 32-G524",
  addressLevel2: "Cambridge",
  addressLevel1: "MA",
  postalCode: "02139",
  country: "US",
  tel: "+1 617 253 5702",
  email: "timbl@w3.org",
};

function camelCase(str) {
  return str.toLowerCase().replace(/-([a-z])/g, s => s[1].toUpperCase());
}

add_task(function* test_populateFieldValues() {
  FormAutofillParent.init();

  let store = FormAutofillParent.getProfileStore();
  do_check_neq(store, null);

  store.get = function(guid) {
    do_check_eq(guid, TEST_GUID);
    return store._clone(TEST_PROFILE);
  };

  let notifyUsedCalledCount = 0;
  store.notifyUsed = function(guid) {
    do_check_eq(guid, TEST_GUID);
    notifyUsedCalledCount++;
  };

  yield new Promise((resolve) => {
    FormAutofillParent.receiveMessage({
      name: "FormAutofill:PopulateFieldValues",
      data: {
        guid: TEST_GUID,
        fields: TEST_FIELDS,
      },
      target: {
        sendAsyncMessage: function(name, data) {
          do_check_eq(name, "FormAutofill:fillForm");

          let fields = data.fields;
          do_check_eq(fields.length, TEST_FIELDS.length);

          for (let i = 0; i < fields.length; i++) {
            do_check_eq(fields[i].fieldName, TEST_FIELDS[i].fieldName);
            do_check_eq(fields[i].value,
              TEST_PROFILE[camelCase(fields[i].fieldName)]);
          }

          resolve();
        },
      },
    });
  });

  do_check_eq(notifyUsedCalledCount, 1);

  FormAutofillParent._uninit();
  do_check_null(FormAutofillParent.getProfileStore());
});

add_task(function* test_populateFieldValues_with_invalid_guid() {
  FormAutofillParent.init();

  Assert.throws(() => {
    FormAutofillParent.receiveMessage({
      name: "FormAutofill:PopulateFieldValues",
      data: {
        guid: "invalid-guid",
        fields: TEST_FIELDS,
      },
      target: {},
    });
  }, /No matching profile\./);

  FormAutofillParent._uninit();
});
