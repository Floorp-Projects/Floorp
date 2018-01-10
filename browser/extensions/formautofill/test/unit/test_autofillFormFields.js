/*
 * Test for form auto fill content helper fill all inputs function.
 */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

Cu.import("resource://formautofill/FormAutofillHandler.jsm");
let {MasterPassword} = Cu.import("resource://formautofill/MasterPassword.jsm", {});

const TESTCASES = [
  {
    description: "Form without autocomplete property",
    document: `<form><input id="given-name"><input id="family-name">
               <input id="street-addr"><input id="city"><select id="country"></select>
               <input id='email'><input id="tel"></form>`,
    focusedInputId: "given-name",
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
    focusedInputId: "given-name",
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
    focusedInputId: "given-name",
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
    focusedInputId: "given-name",
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
    focusedInputId: "given-name",
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
               <input id="given-name" autocomplete="shipping given-name">
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
    focusedInputId: "given-name",
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
               <input id="given-name" autocomplete="shipping given-name">
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
    focusedInputId: "given-name",
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
  {
    description: "Fill address fields in a form with addr and CC fields.",
    document: `<form>
               <input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <input id="street-addr" autocomplete="street-address">
               <input id="city" autocomplete="address-level2">
               <select id="country" autocomplete="country">
                 <option/>
                 <option value="US">United States</option>
               </select>
               <input id="email" autocomplete="email">
               <input id="tel" autocomplete="tel">
               <input id="cc-number" autocomplete="cc-number">
               <input id="cc-name" autocomplete="cc-name">
               <input id="cc-exp-month" autocomplete="cc-exp-month">
               <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    focusedInputId: "given-name",
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
      "cc-number": "",
      "cc-name": "",
      "cc-exp-month": "",
      "cc-exp-year": "",
    },
  },
  {
    description: "Fill credit card fields in a form with addr and CC fields.",
    document: `<form>
               <input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <input id="street-addr" autocomplete="street-address">
               <input id="city" autocomplete="address-level2">
               <select id="country" autocomplete="country">
                 <option/>
                 <option value="US">United States</option>
               </select>
               <input id="email" autocomplete="email">
               <input id="tel" autocomplete="tel">
               <input id="cc-number" autocomplete="cc-number">
               <input id="cc-name" autocomplete="cc-name">
               <input id="cc-exp-month" autocomplete="cc-exp-month">
               <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    focusedInputId: "cc-number",
    profileData: {
      "guid": "123",
      "cc-number": "1234000056780000",
      "cc-name": "test name",
      "cc-exp-month": "06",
      "cc-exp-year": "25",
    },
    expectedResult: {
      "street-addr": "",
      "city": "",
      "country": "",
      "email": "",
      "tel": "",
      "cc-number": "1234000056780000",
      "cc-name": "test name",
      "cc-exp-month": "06",
      "cc-exp-year": "25",
    },
  },


];

const TESTCASES_INPUT_UNCHANGED = [
  {
    description: "Form with autocomplete select elements; with default and no matching options",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <select id="country" autocomplete="shipping country">
                 <option value="US">United States</option>
               </select>
               <select id="state" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="CA">California</option>
                 <option value="WA">Washington</option>
               </select>
               </form>`,
    focusedInputId: "given-name",
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

const TESTCASES_FILL_SELECT = [
  // US States
  {
    description: "Form with US states select elements",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <select id="state" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="CA">California</option>
               </select></form>`,
    focusedInputId: "given-name",
    profileData: {
      "guid": "123",
      "country": "US",
      "address-level1": "CA",
    },
    expectedResult: {
      "state": "CA",
    },
  },
  {
    description: "Form with US states select elements; with lower case state key",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <select id="state" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="ca">ca</option>
               </select></form>`,
    focusedInputId: "given-name",
    profileData: {
      "guid": "123",
      "country": "US",
      "address-level1": "CA",
    },
    expectedResult: {
      "state": "ca",
    },
  },
  {
    description: "Form with US states select elements; with state name and extra spaces",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <select id="state" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="CA">CA</option>
               </select></form>`,
    focusedInputId: "given-name",
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
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <select id="state" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="US-WA">WA-Washington</option>
               </select></form>`,
    focusedInputId: "given-name",
    profileData: {
      "guid": "123",
      "country": "US",
      "address-level1": "WA",
    },
    expectedResult: {
      "state": "US-WA",
    },
  },

  // Country
  {
    description: "Form with country select elements",
    document: `<form>
               <input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <select id="country" autocomplete="country">
                 <option value=""></option>
                 <option value="US">United States</option>
               </select></form>`,
    focusedInputId: "given-name",
    profileData: {
      "guid": "123",
      "country": "US",
    },
    expectedResult: {
      "country": "US",
    },
  },
  {
    description: "Form with country select elements; with lower case key",
    document: `<form>
               <input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <select id="country" autocomplete="country">
                 <option value=""></option>
                 <option value="us">us</option>
               </select></form>`,
    focusedInputId: "given-name",
    profileData: {
      "guid": "123",
      "country": "US",
    },
    expectedResult: {
      "country": "us",
    },
  },
  {
    description: "Form with country select elements; with alternative name 1",
    document: `<form>
               <input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <select id="country" autocomplete="country">
                 <option value=""></option>
                 <option value="XX">United States</option>
               </select></form>`,
    focusedInputId: "given-name",
    profileData: {
      "guid": "123",
      "country": "US",
    },
    expectedResult: {
      "country": "XX",
    },
  },
  {
    description: "Form with country select elements; with alternative name 2",
    document: `<form>
               <input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <select id="country" autocomplete="country">
                 <option value=""></option>
                 <option value="XX">America</option>
               </select></form>`,
    focusedInputId: "given-name",
    profileData: {
      "guid": "123",
      "country": "US",
    },
    expectedResult: {
      "country": "XX",
    },
  },
  {
    description: "Form with country select elements; with partial matching value",
    document: `<form>
               <input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <select id="country" autocomplete="country">
                 <option value=""></option>
                 <option value="XX">Ship to America</option>
               </select></form>`,
    focusedInputId: "given-name",
    profileData: {
      "guid": "123",
      "country": "US",
    },
    expectedResult: {
      "country": "XX",
    },
  },
];

function do_test(testcases, testFn) {
  for (let tc of testcases) {
    (function() {
      let testcase = tc;
      add_task(async function() {
        info("Starting testcase: " + testcase.description);
        let ccNumber = testcase.profileData["cc-number"];
        if (ccNumber) {
          testcase.profileData["cc-number-encrypted"] = await MasterPassword.encrypt(ccNumber);
          delete testcase.profileData["cc-number"];
        }

        let doc = MockDocument.createTestDocument("http://localhost:8080/test/",
                                                  testcase.document);
        let form = doc.querySelector("form");
        let formLike = FormLikeFactory.createFromForm(form);
        let handler = new FormAutofillHandler(formLike);
        let promises = [];
        // Replace the interal decrypt method with MasterPassword API
        let decryptHelper = async (cipherText, reauth) => {
          let string;
          try {
            string = await MasterPassword.decrypt(cipherText, reauth);
          } catch (e) {
            if (e.result != Cr.NS_ERROR_ABORT) {
              throw e;
            }
            info("User canceled master password entry");
          }
          return string;
        };

        handler.collectFormFields();

        let focusedInput = doc.getElementById(testcase.focusedInputId);
        handler.focusedInput = focusedInput;

        for (let section of handler.sections) {
          section._decrypt = decryptHelper;
        }

        handler.activeSection.fieldDetails.forEach(field => {
          let element = field.elementWeakRef.get();
          if (!testcase.profileData[field.fieldName]) {
            // Avoid waiting for `change` event of a input with a blank value to
            // be filled.
            return;
          }
          promises.push(...testFn(testcase, element));
        });

        let [adaptedProfile] = handler.activeSection.getAdaptedProfiles([testcase.profileData]);
        await handler.autofillFormFields(adaptedProfile, focusedInput);
        Assert.equal(handler.activeSection.filledRecordGUID, testcase.profileData.guid,
                     "Check if filledRecordGUID is set correctly");
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

do_test(TESTCASES_FILL_SELECT, (testcase, element) => {
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
