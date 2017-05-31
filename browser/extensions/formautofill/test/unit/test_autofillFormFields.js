/*
 * Test for form auto fill content helper fill all inputs function.
 */

"use strict";

Cu.import("resource://formautofill/FormAutofillHandler.jsm");

const TESTCASES = [
  {
    description: "Form without autocomplete property",
    document: `<form><input id="given-name"><input id="family-name">
               <input id="street-addr"><input id="city"><select id="country"></select>
               <input id='email'><input id="tel"></form>`,
    fieldDetails: [],
    profileData: {},
    expectedResult: {
      "street-addr": "",
      "city": "",
      "country": "",
      "email": "",
      "tel": "",
    },
  },
  {
    description: "Form with autocomplete properties and 1 token",
    document: `<form><input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <input id="street-addr" autocomplete="street-address">
               <input id="city" autocomplete="address-level2">
               <select id="country" autocomplete="country"></select>
               <input id="email" autocomplete="email">
               <input id="tel" autocomplete="tel"></form>`,
    fieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name", "element": {}},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name", "element": {}},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address", "element": {}},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2", "element": {}},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "country", "element": {}},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "email", "element": {}},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel", "element": {}},
    ],
    profileData: {
      "guid": "123",
      "street-address": "2 Harrison St",
      "address-level2": "San Francisco",
      "country": "US",
      "email": "foo@mozilla.com",
      "tel": "1234567",
    },
    expectedResult: {
      "street-addr": "2 Harrison St",
      "city": "San Francisco",
      "country": "US",
      "email": "foo@mozilla.com",
      "tel": "1234567",
    },
  },
  {
    description: "Form with autocomplete properties and 2 tokens",
    document: `<form><input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-addr" autocomplete="shipping street-address">
               <input id="city" autocomplete="shipping address-level2">
               <select id="country" autocomplete="shipping country"></select>
               <input id='email' autocomplete="shipping email">
               <input id="tel" autocomplete="shipping tel"></form>`,
    fieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel", "element": {}},
    ],
    profileData: {
      "guid": "123",
      "street-address": "2 Harrison St",
      "address-level2": "San Francisco",
      "country": "US",
      "email": "foo@mozilla.com",
      "tel": "1234567",
    },
    expectedResult: {
      "street-addr": "2 Harrison St",
      "city": "San Francisco",
      "country": "US",
      "email": "foo@mozilla.com",
      "tel": "1234567",
    },
  },
  {
    description: "Form with autocomplete properties and profile is partly matched",
    document: `<form><input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-addr" autocomplete="shipping street-address">
               <input id="city" autocomplete="shipping address-level2">
               <input id="country" autocomplete="shipping country">
               <input id='email' autocomplete="shipping email">
               <input id="tel" autocomplete="shipping tel"></form>`,
    fieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel", "element": {}},
    ],
    profileData: {
      "guid": "123",
      "street-address": "2 Harrison St",
      "address-level2": "San Francisco",
      "country": "US",
      "email": "",
      "tel": "",
    },
    expectedResult: {
      "street-addr": "2 Harrison St",
      "city": "San Francisco",
      "country": "US",
      "email": "",
      "tel": "",
    },
  },
  {
    description: "Form with autocomplete properties but mismatched",
    document: `<form><input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-addr" autocomplete="billing street-address">
               <input id="city" autocomplete="billing address-level2">
               <input id="country" autocomplete="billing country">
               <input id='email' autocomplete="shipping email">
               <input id="tel" autocomplete="shipping tel"></form>`,
    fieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "organization", "element": null},
    ],
    profileData: {
      "guid": "123",
      "street-address": "",
      "address-level2": "",
      "country": "",
      "email": "foo@mozilla.com",
      "tel": "1234567",
    },
    expectedResult: {
      "street-addr": "",
      "city": "",
      "country": "",
      "email": "foo@mozilla.com",
      "tel": "1234567",
    },
  },
];

for (let tc of TESTCASES) {
  (function() {
    let testcase = tc;
    add_task(async function() {
      do_print("Starting testcase: " + testcase.description);

      let doc = MockDocument.createTestDocument("http://localhost:8080/test/",
                                                testcase.document);
      let form = doc.querySelector("form");
      let handler = new FormAutofillHandler(form);
      let onChangePromises = [];

      handler.fieldDetails = testcase.fieldDetails;
      handler.fieldDetails.forEach((field, index) => {
        let element = doc.querySelectorAll("input, select")[index];
        field.elementWeakRef = Cu.getWeakReference(element);
        if (element instanceof Ci.nsIDOMHTMLSelectElement) {
          // TODO: Bug 1364823 should remove the condition and handle filling
          // value in <select>
          return;
        }
        if (!testcase.profileData[field.fieldName]) {
          // Avoid waiting for `change` event of a input with a blank value to
          // be filled.
          return;
        }
        onChangePromises.push(new Promise(resolve => {
          element.addEventListener("change", () => {
            let id = element.id;
            Assert.equal(element.value, testcase.expectedResult[id],
                        "Check the " + id + " fields were filled with correct data");
            resolve();
          }, {once: true});
        }));
      });

      handler.autofillFormFields(testcase.profileData);

      Assert.equal(handler.filledProfileGUID, testcase.profileData.guid,
                   "Check if filledProfileGUID is set correctly");
      await Promise.all(onChangePromises);
    });
  })();
}
