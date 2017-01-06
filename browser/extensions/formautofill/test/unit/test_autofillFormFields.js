/*
 * Test for form auto fill content helper fill all inputs function.
 */

"use strict";

let {FormAutofillHandler} = loadFormAutofillContent();

const TESTCASES = [
  {
    description: "Form without autocomplete property",
    document: `<form><input id="given-name"><input id="family-name">
               <input id="street-addr"><input id="city"><input id="country">
               <input id='email'><input id="tel"></form>`,
    fieldDetails: [],
    profileData: [],
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
               <input id="country" autocomplete="country">
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
    profileData: [
      {"section": "", "addressType": "", "fieldName": "given-name", "contactType": "", "index": 0, "value": "foo"},
      {"section": "", "addressType": "", "fieldName": "family-name", "contactType": "", "index": 1, "value": "bar"},
      {"section": "", "addressType": "", "fieldName": "street-address", "contactType": "", "index": 2, "value": "2 Harrison St"},
      {"section": "", "addressType": "", "fieldName": "address-level2", "contactType": "", "index": 3, "value": "San Francisco"},
      {"section": "", "addressType": "", "fieldName": "country", "contactType": "", "index": 4, "value": "US"},
      {"section": "", "addressType": "", "fieldName": "email", "contactType": "", "index": 5, "value": "foo@mozilla.com"},
      {"section": "", "addressType": "", "fieldName": "tel", "contactType": "", "index": 6, "value": "1234567"},
    ],
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
    profileData: [
      {"section": "", "addressType": "shipping", "fieldName": "given-name", "contactType": "", "index": 0, "value": "foo"},
      {"section": "", "addressType": "shipping", "fieldName": "family-name", "contactType": "", "index": 1, "value": "bar"},
      {"section": "", "addressType": "shipping", "fieldName": "street-address", "contactType": "", "index": 2, "value": "2 Harrison St"},
      {"section": "", "addressType": "shipping", "fieldName": "address-level2", "contactType": "", "index": 3, "value": "San Francisco"},
      {"section": "", "addressType": "shipping", "fieldName": "country", "contactType": "", "index": 4, "value": "US"},
      {"section": "", "addressType": "shipping", "fieldName": "email", "contactType": "", "index": 5, "value": "foo@mozilla.com"},
      {"section": "", "addressType": "shipping", "fieldName": "tel", "contactType": "", "index": 6, "value": "1234567"},
    ],
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
    profileData: [
      {"section": "", "addressType": "shipping", "fieldName": "given-name", "contactType": "", "index": 0, "value": "foo"},
      {"section": "", "addressType": "shipping", "fieldName": "family-name", "contactType": "", "index": 1, "value": "bar"},
      {"section": "", "addressType": "shipping", "fieldName": "street-address", "contactType": "", "index": 2, "value": "2 Harrison St"},
      {"section": "", "addressType": "shipping", "fieldName": "address-level2", "contactType": "", "index": 3, "value": "San Francisco"},
      {"section": "", "addressType": "shipping", "fieldName": "country", "contactType": "", "index": 4, "value": "US"},
      {"section": "", "addressType": "shipping", "fieldName": "email", "contactType": "", "index": 5},
      {"section": "", "addressType": "shipping", "fieldName": "tel", "contactType": "", "index": 6},
    ],
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
    ],
    profileData: [
      {"section": "", "addressType": "shipping", "fieldName": "given-name", "contactType": "", "index": 0, "value": "foo"},
      {"section": "", "addressType": "shipping", "fieldName": "family-name", "contactType": "", "index": 1, "value": "bar"},
      {"section": "", "addressType": "shipping", "fieldName": "street-address", "contactType": "", "index": 2, "value": "2 Harrison St"},
      {"section": "", "addressType": "shipping", "fieldName": "address-level2", "contactType": "", "index": 3, "value": "San Francisco"},
      {"section": "", "addressType": "shipping", "fieldName": "country", "contactType": "", "index": 4, "value": "US"},
      {"section": "", "addressType": "shipping", "fieldName": "email", "contactType": "", "index": 5, "value": "foo@mozilla.com"},
      {"section": "", "addressType": "shipping", "fieldName": "tel", "contactType": "", "index": 6, "value": "1234567"},
    ],
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
    add_task(function* () {
      do_print("Starting testcase: " + testcase.description);

      let doc = MockDocument.createTestDocument("http://localhost:8080/test/",
                                                testcase.document);
      let form = doc.querySelector("form");
      let handler = new FormAutofillHandler(form);

      handler.fieldDetails = testcase.fieldDetails;
      handler.fieldDetails.forEach((field, index) => {
        field.element = doc.querySelectorAll("input")[index];
      });

      handler.autofillFormFields(testcase.profileData);
      for (let id in testcase.expectedResult) {
        Assert.equal(doc.getElementById(id).value, testcase.expectedResult[id],
                    "Check the " + id + " fields were filled with correct data");
      }
    });
  })();
}
