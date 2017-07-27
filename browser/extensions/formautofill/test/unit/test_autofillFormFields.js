/*
 * Test for form auto fill content helper fill all inputs function.
 */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

Cu.import("resource://formautofill/FormAutofillHandler.jsm");

const TESTCASES = [
  {
    description: "Form without autocomplete property",
    document: `<form><input id="given-name"><input id="family-name">
               <input id="street-addr"><input id="city"><select id="country"></select>
               <input id='email'><input id="tel"></form>`,
    addressFieldDetails: [],
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
               <select id="country" autocomplete="country">
                 <option/>
                 <option value="US">United States</option>
               </select>
               <input id="email" autocomplete="email">
               <input id="tel" autocomplete="tel"></form>`,
    addressFieldDetails: [
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
      "street-address": "2 Harrison St line2",
      "-moz-street-address-one-line": "2 Harrison St line2",
      "address-level2": "San Francisco",
      "country": "US",
      "email": "foo@mozilla.com",
      "tel": "1234567",
    },
    expectedResult: {
      "street-addr": "2 Harrison St line2",
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
               <select id="country" autocomplete="shipping country">
                 <option/>
                 <option value="US">United States</option>
               </select>
               <input id='email' autocomplete="shipping email">
               <input id="tel" autocomplete="shipping tel"></form>`,
    addressFieldDetails: [
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
    addressFieldDetails: [
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
    addressFieldDetails: [
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
  {
    description: "Form with autocomplete select elements and matching option values",
    document: `<form>
               <select id="country" autocomplete="shipping country">
                 <option value=""></option>
                 <option value="US">United States</option>
               </select>
               <select id="state" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="CA">California</option>
                 <option value="WA">Washington</option>
               </select>
               </form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level1", "element": {}},
    ],
    profileData: {
      "guid": "123",
      "country": "US",
      "address-level1": "CA",
    },
    expectedResult: {
      "country": "US",
      "state": "CA",
    },
  },
  {
    description: "Form with autocomplete select elements and matching option texts",
    document: `<form>
               <select id="country" autocomplete="shipping country">
                 <option value=""></option>
                 <option value="US">United States</option>
               </select>
               <select id="state" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="CA">California</option>
                 <option value="WA">Washington</option>
               </select>
               </form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level1", "element": {}},
    ],
    profileData: {
      "guid": "123",
      "country": "United States",
      "address-level1": "California",
    },
    expectedResult: {
      "country": "US",
      "state": "CA",
    },
  },
];

const TESTCASES_INPUT_UNCHANGED = [
  {
    description: "Form with autocomplete select elements; with default and no matching options",
    document: `<form>
               <select id="country" autocomplete="shipping country">
                 <option value="US">United States</option>
               </select>
               <select id="state" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="CA">California</option>
                 <option value="WA">Washington</option>
               </select>
               </form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country", "element": {}},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level1", "element": {}},
    ],
    profileData: {
      "guid": "123",
      "country": "US",
      "address-level1": "unknown state",
    },
    expectedResult: {
      "country": "US",
      "state": "",
    },
  },
];

const TESTCASES_US_STATES = [
  {
    description: "Form with US states select elements; with lower case state key",
    document: `<form><select id="state" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="CA">California</option>
               </select></form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level1", "element": {}},
    ],
    profileData: {
      "guid": "123",
      "country": "US",
      "address-level1": "ca",
    },
    expectedResult: {
      "state": "CA",
    },
  },
  {
    description: "Form with US states select elements; with state name and extra spaces",
    document: `<form><select id="state" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="CA">CA</option>
               </select></form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level1", "element": {}},
    ],
    profileData: {
      "guid": "123",
      "country": "US",
      "address-level1": " California ",
    },
    expectedResult: {
      "state": "CA",
    },
  },
  {
    description: "Form with US states select elements; with partial state key match",
    document: `<form><select id="state" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="US-WA">WA-Washington</option>
               </select></form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level1", "element": {}},
    ],
    profileData: {
      "guid": "123",
      "country": "US",
      "address-level1": "WA",
    },
    expectedResult: {
      "state": "US-WA",
    },
  },
];

function do_test(testcases, testFn) {
  for (let tc of testcases) {
    (function() {
      let testcase = tc;
      add_task(async function() {
        do_print("Starting testcase: " + testcase.description);

        let doc = MockDocument.createTestDocument("http://localhost:8080/test/",
                                                  testcase.document);
        let form = doc.querySelector("form");
        let formLike = FormLikeFactory.createFromForm(form);
        let handler = new FormAutofillHandler(formLike);
        let promises = [];

        handler.addressFieldDetails = testcase.addressFieldDetails;
        handler.addressFieldDetails.forEach((field, index) => {
          let element = doc.querySelectorAll("input, select")[index];
          field.elementWeakRef = Cu.getWeakReference(element);
          if (!testcase.profileData[field.fieldName]) {
            // Avoid waiting for `change` event of a input with a blank value to
            // be filled.
            return;
          }
          promises.push(...testFn(testcase, element));
        });

        handler.autofillFormFields(testcase.profileData);
        Assert.equal(handler.filledProfileGUID, testcase.profileData.guid,
                     "Check if filledProfileGUID is set correctly");
        await Promise.all(promises);
      });
    })();
  }
}

do_test(TESTCASES, (testcase, element) => {
  let id = element.id;
  return [
    new Promise(resolve => {
      element.addEventListener("input", () => {
        Assert.ok(true, "Checking " + id + " field fires input event");
        resolve();
      }, {once: true});
    }),
    new Promise(resolve => {
      element.addEventListener("change", () => {
        Assert.ok(true, "Checking " + id + " field fires change event");
        Assert.equal(element.value, testcase.expectedResult[id],
                    "Check the " + id + " field was filled with correct data");
        resolve();
      }, {once: true});
    }),
  ];
});

do_test(TESTCASES_INPUT_UNCHANGED, (testcase, element) => {
  return [
    new Promise((resolve, reject) => {
      // Make sure no change or input event is fired when no change occurs.
      let cleaner;
      let timer = setTimeout(() => {
        let id = element.id;
        element.removeEventListener("change", cleaner);
        element.removeEventListener("input", cleaner);
        Assert.equal(element.value, testcase.expectedResult[id],
                    "Check no value is changed on the " + id + " field");
        resolve();
      }, 1000);
      cleaner = event => {
        clearTimeout(timer);
        reject(`${event.type} event should not fire`);
      };
      element.addEventListener("change", cleaner);
      element.addEventListener("input", cleaner);
    }),
  ];
});

do_test(TESTCASES_US_STATES, (testcase, element) => {
  let id = element.id;
  return [
    new Promise(resolve => {
      element.addEventListener("input", () => {
        Assert.equal(element.value, testcase.expectedResult[id],
                    "Check the " + id + " field was filled with correct data");
        resolve();
      }, {once: true});
    }),
  ];
});
