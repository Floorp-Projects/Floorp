/*
 * Test for form auto fill content helper fill all inputs function.
 */

"use strict";

Cu.import("resource://formautofill/FormAutofillHandler.jsm");

const DEFAULT_PROFILE = {
  "guid": "123",
  "street-address": "2 Harrison St\nline2\nline3",
  "address-line1": "2 Harrison St",
  "address-line2": "line2",
  "address-line3": "line3",
};

const TESTCASES = [
  {
    description: "Form with street-address",
    document: `<form>
               <input id="street-addr" autocomplete="street-address">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_PROFILE)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St line2 line3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2",
      "address-line3": "line3",
    }],
  },
  {
    description: "Form with street-address, address-line[1, 2, 3]",
    document: `<form>
               <input id="street-addr" autocomplete="street-address">
               <input id="line1" autocomplete="address-line1">
               <input id="line2" autocomplete="address-line2">
               <input id="line3" autocomplete="address-line3">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_PROFILE)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St line2 line3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2",
      "address-line3": "line3",
    }],
  },
  {
    description: "Form with street-address, address-line1",
    document: `<form>
               <input id="street-addr" autocomplete="street-address">
               <input id="line1" autocomplete="address-line1">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_PROFILE)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St line2 line3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St line2 line3",
      "address-line2": "line2",
      "address-line3": "line3",
    }],
  },
  {
    description: "Form with street-address, address-line[1, 2]",
    document: `<form>
               <input id="street-addr" autocomplete="street-address">
               <input id="line1" autocomplete="address-line1">
               <input id="line2" autocomplete="address-line2">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_PROFILE)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St line2 line3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2 line3",
      "address-line3": "line3",
    }],
  },
  {
    description: "Form with street-address, address-line[1, 3]",
    document: `<form>
               <input id="street-addr" autocomplete="street-address">
               <input id="line1" autocomplete="address-line1">
               <input id="line3" autocomplete="address-line3">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_PROFILE)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St line2 line3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2 line3",
      "address-line3": "line3",
    }],
  },
];

for (let testcase of TESTCASES) {
  add_task(async function() {
    do_print("Starting testcase: " + testcase.description);

    let doc = MockDocument.createTestDocument("http://localhost:8080/test/",
                                              testcase.document);
    let form = doc.querySelector("form");
    let formLike = FormLikeFactory.createFromForm(form);
    let handler = new FormAutofillHandler(formLike);

    handler.collectFormFields();
    let adaptedAddresses = handler.getAdaptedProfiles(testcase.profileData);
    Assert.deepEqual(adaptedAddresses, testcase.expectedResult);
  });
}

