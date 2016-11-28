/*
 * Test for form auto fill content helper collectFormFields functions.
 */

"use strict";

let {FormAutofillHandler} = Cu.import("resource://formautofill/FormAutofillContent.jsm");

const TESTCASES = [
  {
    description: "Form without autocomplete property",
    document: `<form><input id="given-name"><input id="family-name">
               <input id="street-addr"><input id="city"><input id="country">
               <input id='email'><input id="tel"></form>`,
    returnedFormat: [],
    fieldDetails: [],
  },
  {
    description: "Form with autocomplete properties and 1 token",
    document: `<form><input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <input id="street-addr" autocomplete="street-address">
               <input id="city" autocomplete="address-level2">
               <input id="country" autocomplete="country">
               <input id="email" autocomplete="email">
               <input id="tel" autocomplete="tel"></form>`,
    returnedFormat: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name", "index": 0},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name", "index": 1},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address", "index": 2},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2", "index": 3},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "country", "index": 4},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "email", "index": 5},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel", "index": 6},
    ],
    fieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name", "element": {}},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name", "element": {}},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address", "element": {}},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2", "element": {}},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "country", "element": {}},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "email", "element": {}},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel", "element": {}},
    ],
  },
  {
    description: "Form with autocomplete properties and 2 tokens",
    document: `<form><input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-addr" autocomplete="shipping street-address">
               <input id="city" autocomplete="shipping address-level2">
               <input id="country" autocomplete="shipping country">
               <input id='email' autocomplete="shipping email">
               <input id="tel" autocomplete="shipping tel"></form>`,
    returnedFormat: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name", "index": 0},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name", "index": 1},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address", "index": 2},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2", "index": 3},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country", "index": 4},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email", "index": 5},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel", "index": 6},
    ],
    fieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel", "element": {}},
    ],
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
    returnedFormat: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name", "index": 0},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name", "index": 1},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address", "index": 2},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2", "index": 3},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country", "index": 4},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email", "index": 5},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel", "index": 6},
    ],
    fieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel", "element": {}},
    ],
  },
];

for (let tc of TESTCASES) {
  (function() {
    let testcase = tc;
    add_task(function* () {
      do_print("Starting testcase: " + testcase.description);

      let doc = MockDocument.createTestDocument("http://localhost:8080/test/",
                                                testcase.document);
      let form = doc.querySelector("form");
      let handler = new FormAutofillHandler(form);

      Assert.deepEqual(handler.collectFormFields(), testcase.returnedFormat,
                         "Check the format of form autofill were returned correctly");

      Assert.deepEqual(handler.fieldDetails, testcase.fieldDetails,
                         "Check the fieldDetails were set correctly");
    });
  })();
}
