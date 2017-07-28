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
  "address-level1": "CA",
  "country": "US",
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
      "address-level1": "CA",
      "country": "US",
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
      "address-level1": "CA",
      "country": "US",
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
      "address-level1": "CA",
      "country": "US",
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
      "address-level1": "CA",
      "country": "US",
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
      "address-level1": "CA",
      "country": "US",
    }],
  },
  {
    description: "Form with exact matching options in select",
    document: `<form>
               <select autocomplete="address-level1">
                 <option id="option-address-level1-XX" value="XX">Dummy</option>
                 <option id="option-address-level1-CA" value="CA">California</option>
               </select>
               <select autocomplete="country">
                 <option id="option-country-XX" value="XX">Dummy</option>
                 <option id="option-country-US" value="US">United States</option>
               </select>
               </form>`,
    profileData: [Object.assign({}, DEFAULT_PROFILE)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
    }],
    expectedOptionElements: [{
      "address-level1": "option-address-level1-CA",
      "country": "option-country-US",
    }],
  },
  {
    description: "Form with inexact matching options in select",
    document: `<form>
               <select autocomplete="address-level1">
                 <option id="option-address-level1-XX" value="XX">Dummy</option>
                 <option id="option-address-level1-OO" value="OO">California</option>
               </select>
               <select autocomplete="country">
                 <option id="option-country-XX" value="XX">Dummy</option>
                 <option id="option-country-OO" value="OO">United States</option>
               </select>
               </form>`,
    profileData: [Object.assign({}, DEFAULT_PROFILE)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
    }],
    expectedOptionElements: [{
      "address-level1": "option-address-level1-OO",
      "country": "option-country-OO",
    }],
  },
  {
    description: "Form with value-omitted options in select",
    document: `<form>
               <select autocomplete="address-level1">
                 <option id="option-address-level1-1" value="">Dummy</option>
                 <option id="option-address-level1-2" value="">California</option>
               </select>
               <select autocomplete="country">
                 <option id="option-country-1" value="">Dummy</option>
                 <option id="option-country-2" value="">United States</option>
               </select>
               </form>`,
    profileData: [Object.assign({}, DEFAULT_PROFILE)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
    }],
    expectedOptionElements: [{
      "address-level1": "option-address-level1-2",
      "country": "option-country-2",
    }],
  },
  {
    description: "Form with options with the same value in select",
    document: `<form>
               <select autocomplete="address-level1">
                 <option id="option-address-level1-same1" value="same">Dummy</option>
                 <option id="option-address-level1-same2" value="same">California</option>
               </select>
               <select autocomplete="country">
                 <option id="option-country-same1" value="sametoo">Dummy</option>
                 <option id="option-country-same2" value="sametoo">United States</option>
               </select>
               </form>`,
    profileData: [Object.assign({}, DEFAULT_PROFILE)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2",
      "address-line3": "line3",
      "address-level1": "CA",
      "country": "US",
    }],
    expectedOptionElements: [{
      "address-level1": "option-address-level1-same2",
      "country": "option-country-same2",
    }],
  },
  {
    description: "Form without matching options in select",
    document: `<form>
               <select autocomplete="address-level1">
                 <option id="option-address-level1-dummy1" value="">Dummy</option>
                 <option id="option-address-level1-dummy2" value="">Dummy 2</option>
               </select>
               <select autocomplete="country">
                 <option id="option-country-dummy1" value="">Dummy</option>
                 <option id="option-country-dummy2" value="">Dummy 2</option>
               </select>
               </form>`,
    profileData: [Object.assign({}, DEFAULT_PROFILE)],
    expectedResult: [{
      "guid": "123",
      "street-address": "2 Harrison St\nline2\nline3",
      "-moz-street-address-one-line": "2 Harrison St line2 line3",
      "address-line1": "2 Harrison St",
      "address-line2": "line2",
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

    if (testcase.expectedOptionElements) {
      testcase.expectedOptionElements.forEach((expectedOptionElement, i) => {
        for (let field in expectedOptionElement) {
          let select = form.querySelector(`[autocomplete=${field}]`);
          let expectedOption = doc.getElementById(expectedOptionElement[field]);
          Assert.notEqual(expectedOption, null);

          let value = testcase.profileData[i][field];
          let cache = handler._cacheValue.matchingSelectOption.get(select);
          let targetOption = cache[value] && cache[value].get();
          Assert.notEqual(targetOption, null);

          Assert.equal(targetOption, expectedOption);
        }
      });
    }
  });
}
