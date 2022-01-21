/*
 * Test for form auto fill content helper fill all inputs function.
 */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

const { setTimeout, clearTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm",
  {}
);

var FormAutofillHandler, OSKeyStore;
add_task(async function setup() {
  ({ FormAutofillHandler } = ChromeUtils.import(
    "resource://autofill/FormAutofillHandler.jsm"
  ));
  ({ OSKeyStore } = ChromeUtils.import(
    "resource://gre/modules/OSKeyStore.jsm"
  ));
});

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
      city: "",
      country: "",
      email: "",
      tel: "",
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
      guid: "123",
      "street-address": "2 Harrison St line2",
      "-moz-street-address-one-line": "2 Harrison St line2",
      "address-level2": "San Francisco",
      country: "US",
      email: "foo@mozilla.com",
      tel: "1234567",
    },
    expectedResult: {
      "street-addr": "2 Harrison St line2",
      city: "San Francisco",
      country: "US",
      email: "foo@mozilla.com",
      tel: "1234567",
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
      guid: "123",
      "street-address": "2 Harrison St",
      "address-level2": "San Francisco",
      country: "US",
      email: "foo@mozilla.com",
      tel: "1234567",
    },
    expectedResult: {
      "street-addr": "2 Harrison St",
      city: "San Francisco",
      country: "US",
      email: "foo@mozilla.com",
      tel: "1234567",
    },
  },
  {
    description:
      "Form with autocomplete properties and profile is partly matched",
    document: `<form><input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-addr" autocomplete="shipping street-address">
               <input id="city" autocomplete="shipping address-level2">
               <input id="country" autocomplete="shipping country">
               <input id='email' autocomplete="shipping email">
               <input id="tel" autocomplete="shipping tel"></form>`,
    focusedInputId: "given-name",
    profileData: {
      guid: "123",
      "street-address": "2 Harrison St",
      "address-level2": "San Francisco",
      country: "US",
      email: "",
      tel: "",
    },
    expectedResult: {
      "street-addr": "2 Harrison St",
      city: "San Francisco",
      country: "US",
      email: "",
      tel: "",
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
      guid: "123",
      "street-address": "",
      "address-level2": "",
      country: "",
      email: "foo@mozilla.com",
      tel: "1234567",
    },
    expectedResult: {
      "street-addr": "",
      city: "",
      country: "",
      email: "foo@mozilla.com",
      tel: "1234567",
    },
  },
  {
    description:
      "Form with autocomplete select elements and matching option values",
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
      guid: "123",
      country: "US",
      "address-level1": "CA",
    },
    expectedResult: {
      country: "US",
      state: "CA",
    },
  },
  {
    description:
      "Form with autocomplete select elements and matching option texts",
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
      guid: "123",
      country: "United States",
      "address-level1": "California",
    },
    expectedResult: {
      country: "US",
      state: "CA",
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
      guid: "123",
      "street-address": "2 Harrison St line2",
      "-moz-street-address-one-line": "2 Harrison St line2",
      "address-level2": "San Francisco",
      country: "US",
      email: "foo@mozilla.com",
      tel: "1234567",
    },
    expectedResult: {
      "street-addr": "2 Harrison St line2",
      city: "San Francisco",
      country: "US",
      email: "foo@mozilla.com",
      tel: "1234567",
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
      guid: "123",
      "cc-number": "4111111111111111",
      "cc-name": "test name",
      "cc-exp-month": 6,
      "cc-exp-year": 25,
    },
    expectedResult: {
      "street-addr": "",
      city: "",
      country: "",
      email: "",
      tel: "",
      "cc-number": "4111111111111111",
      "cc-name": "test name",
      "cc-exp-month": "6",
      "cc-exp-year": "25",
    },
  },
  {
    description:
      "Fill credit card fields in a form with a placeholder on expiration month input field",
    document: `<form>
              <input id="cc-number" autocomplete="cc-number">
              <input id="cc-name" autocomplete="cc-name">
              <input id="cc-exp-month" autocomplete="cc-exp-month" placeholder="MM">
              <input id="cc-exp-year" autocomplete="cc-exp-year">
              </form>
              `,
    focusedInputId: "cc-number",
    profileData: {
      guid: "123",
      "cc-number": "4111111111111111",
      "cc-name": "test name",
      "cc-exp-month": 6,
      "cc-exp-year": 25,
    },
    expectedResult: {
      "cc-number": "4111111111111111",
      "cc-name": "test name",
      "cc-exp-month": "06",
      "cc-exp-year": "25",
    },
  },
  {
    description:
      "Fill credit card fields in a form without a placeholder on expiration month input field",
    document: `<form>
              <input id="cc-number" autocomplete="cc-number">
              <input id="cc-name" autocomplete="cc-name">
              <input id="cc-exp-month" autocomplete="cc-exp-month">
              <input id="cc-exp-year" autocomplete="cc-exp-year">
              </form>
              `,
    focusedInputId: "cc-number",
    profileData: {
      guid: "123",
      "cc-number": "4111111111111111",
      "cc-name": "test name",
      "cc-exp-month": 6,
      "cc-exp-year": 25,
    },
    expectedResult: {
      "cc-number": "4111111111111111",
      "cc-name": "test name",
      "cc-exp-month": "6",
      "cc-exp-year": "25",
    },
  },
  {
    description:
      "Form with hidden input and visible input that share the same autocomplete attribute",
    document: `<form>
               <input id="hidden-cc" autocomplete="cc-number" hidden>
               <input id="hidden-cc-2" autocomplete="cc-number" style="display:none">
               <input id="visible-cc" autocomplete="cc-number">
               <input id="hidden-name" autocomplete="cc-name" hidden>
               <input id="hidden-name-2" autocomplete="cc-name" style="display:none">
               <input id="visible-name" autocomplete="cc-name">
               <input id="cc-exp-month" autocomplete="cc-exp-month">
               <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    focusedInputId: "visible-cc",
    profileData: {
      guid: "123",
      "cc-number": "4111111111111111",
      "cc-name": "test name",
      "cc-exp-month": 6,
      "cc-exp-year": 25,
    },
    expectedResult: {
      guid: "123",
      "visible-cc": "4111111111111111",
      "visible-name": "test name",
      "cc-exp-month": "6",
      "cc-exp-year": "25",
      "hidden-cc": undefined,
      "hidden-cc-2": undefined,
      "hidden-name": undefined,
      "hidden-name-2": undefined,
    },
  },
  {
    description:
      "Fill credit card fields in a form where the value property is being used as a placeholder for cardholder name",
    document: `<form>
              <input id="cc-number" autocomplete="cc-number">
              <input id="cc-name" autocomplete="cc-name" value="JOHN DOE">
              <input id="cc-exp-month" autocomplete="cc-exp-month">
              <input id="cc-exp-year" autocomplete="cc-exp-year">
              </form>`,
    focusedInputId: "cc-number",
    profileData: {
      guid: "123",
      "cc-number": "4111111111111111",
      "cc-name": "test name",
      "cc-exp-month": 6,
      "cc-exp-year": 25,
    },
    expectedResult: {
      guid: "123",
      "cc-number": "4111111111111111",
      "cc-name": "test name",
      "cc-exp-month": "6",
      "cc-exp-year": "25",
    },
  },
  {
    description:
      "Fill credit card number fields in a form with multiple cc-number inputs",
    document: `<form>
                <input id="cc-number1" maxlength="4">
                <input id="cc-number2" maxlength="4">
                <input id="cc-number3" maxlength="4">
                <input id="cc-number4" maxlength="4">
                <input id="cc-exp-month" autocomplete="cc-exp-month">
                <input id="cc-exp-year" autocomplete="cc-exp-year">
                </form>`,
    focusedInputId: "cc-number1",
    profileData: {
      guid: "123",
      "cc-number": "371449635398431",
      "cc-exp-month": 6,
      "cc-exp-year": 25,
    },
    expectedResult: {
      guid: "123",
      "cc-number1": "3714",
      "cc-number2": "4963",
      "cc-number3": "5398",
      "cc-number4": "431",
      "cc-exp-month": "6",
      "cc-exp-year": "25",
    },
  },
  {
    description:
      "Fill credit card number fields in a form with multiple valid credit card sections",
    document: `<form>
                <input id="cc-type1">
                <input id="cc-number1" maxlength="4">
                <input id="cc-number2" maxlength="4">
                <input id="cc-number3" maxlength="4">
                <input id="cc-number4" maxlength="4">
                <input id="cc-exp-month1">
                <input id="cc-exp-year1">
                <input id="cc-type2">
                <input id="cc-number5" maxlength="4">
                <input id="cc-number6" maxlength="4">
                <input id="cc-number7" maxlength="4">
                <input id="cc-number8" maxlength="4">
                <input id="cc-exp-month2">
                <input id="cc-exp-year2">
                <input>
                <input>
                <input>
              </form>
                `,
    focusedInputId: "cc-number1",
    profileData: {
      guid: "123",
      "cc-type": "mastercard",
      "cc-number": "371449635398431",
      "cc-exp-month": 6,
      "cc-exp-year": 25,
    },
    expectedResult: {
      guid: "123",
      "cc-type1": "mastercard",
      "cc-number1": "3714",
      "cc-number2": "4963",
      "cc-number3": "5398",
      "cc-number4": "431",
      "cc-exp-month1": "6",
      "cc-exp-year1": "25",
      "cc-type2": "",
      "cc-number-5": "",
      "cc-number-6": "",
      "cc-number-7": "",
      "cc-number-8": "",
      "cc-exp-month2": "",
      "cc-exp-year2": "",
    },
  },
];

const TESTCASES_INPUT_UNCHANGED = [
  {
    description:
      "Form with autocomplete select elements; with default and no matching options",
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
      guid: "123",
      country: "US",
      "address-level1": "unknown state",
    },
    expectedResult: {
      country: "US",
      state: "",
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
      guid: "123",
      country: "US",
      "address-level1": "CA",
    },
    expectedResult: {
      state: "CA",
    },
  },
  {
    description:
      "Form with US states select elements; with lower case state key",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <select id="state" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="ca">ca</option>
               </select></form>`,
    focusedInputId: "given-name",
    profileData: {
      guid: "123",
      country: "US",
      "address-level1": "CA",
    },
    expectedResult: {
      state: "ca",
    },
  },
  {
    description:
      "Form with US states select elements; with state name and extra spaces",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <select id="state" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="CA">CA</option>
               </select></form>`,
    focusedInputId: "given-name",
    profileData: {
      guid: "123",
      country: "US",
      "address-level1": " California ",
    },
    expectedResult: {
      state: "CA",
    },
  },
  {
    description:
      "Form with US states select elements; with partial state key match",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <select id="state" autocomplete="shipping address-level1">
                 <option value=""></option>
                 <option value="US-WA">WA-Washington</option>
               </select></form>`,
    focusedInputId: "given-name",
    profileData: {
      guid: "123",
      country: "US",
      "address-level1": "WA",
    },
    expectedResult: {
      state: "US-WA",
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
      guid: "123",
      country: "US",
    },
    expectedResult: {
      country: "US",
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
      guid: "123",
      country: "US",
    },
    expectedResult: {
      country: "us",
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
      guid: "123",
      country: "US",
    },
    expectedResult: {
      country: "XX",
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
      guid: "123",
      country: "US",
    },
    expectedResult: {
      country: "XX",
    },
  },
  {
    description:
      "Form with country select elements; with partial matching value",
    document: `<form>
               <input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <select id="country" autocomplete="country">
                 <option value=""></option>
                 <option value="XX">Ship to America</option>
               </select></form>`,
    focusedInputId: "given-name",
    profileData: {
      guid: "123",
      country: "US",
    },
    expectedResult: {
      country: "XX",
    },
  },
  {
    description:
      "Fill credit card expiration month field in a form with select field",
    document: `<form>
              <input id="cc-number" autocomplete="cc-number">
              <input id="cc-name" autocomplete="cc-name">
              <select id="cc-exp-month" autocomplete="cc-exp-month">
                <option value="">MM</option>
                <option value="6">06</option>
              </select></form>`,
    focusedInputId: "cc-number",
    profileData: {
      guid: "123",
      "cc-number": "4111111111111111",
      "cc-name": "test name",
      "cc-exp-month": 6,
      "cc-exp-year": 25,
    },
    expectedResult: {
      "cc-number": "4111111111111111",
      "cc-name": "test name",
      "cc-exp-month": "6",
      "cc-exp-year": "2025",
    },
  },
  {
    description:
      "Fill credit card information correctly when one of the card type options is 'American Express'",
    document: `<form>
                <select id="cc-type" autocomplete="cc-type">
                  <option value="">Please select</option>
                  <option value="MA">Mastercard</option>
                  <option value="AX">American Express</option>
                </select>
                <input id="cc-number" autocomplete="cc-number">
                <input id="cc-name" autocomplete="cc-name">
                <input id="cc-exp-month" autocomplete="cc-exp-month">
                <input id="cc-exp-year" autocomplete="cc-exp-year">
              </form>`,
    focusedInputId: "cc-number",
    profileData: {
      guid: "123",
      "cc-number": "378282246310005",
      "cc-type": "amex",
      "cc-name": "test name",
      "cc-exp-month": 8,
      "cc-exp-year": 26,
    },
    expectedResult: {
      guid: "123",
      "cc-number": "378282246310005",
      "cc-type": "AX",
      "cc-name": "test name",
      "cc-exp-month": 8,
      "cc-exp-year": 26,
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
          testcase.profileData[
            "cc-number-encrypted"
          ] = await OSKeyStore.encrypt(ccNumber);
          delete testcase.profileData["cc-number"];
        }

        let doc = MockDocument.createTestDocument(
          "http://localhost:8080/test/",
          testcase.document
        );
        let form = doc.querySelector("form");
        let formLike = FormLikeFactory.createFromForm(form);
        let handler = new FormAutofillHandler(formLike);
        let promises = [];
        // Replace the internal decrypt method with OSKeyStore API,
        // but don't pass the reauth parameter to avoid triggering
        // reauth login dialog in these tests.
        let decryptHelper = async (cipherText, reauth) => {
          return OSKeyStore.decrypt(cipherText, false);
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

        let [adaptedProfile] = handler.activeSection.getAdaptedProfiles([
          testcase.profileData,
        ]);
        await handler.autofillFormFields(adaptedProfile, focusedInput);
        Assert.equal(
          handler.activeSection.filledRecordGUID,
          testcase.profileData.guid,
          "Check if filledRecordGUID is set correctly"
        );
        await Promise.all(promises);
      });
    })();
  }
}

do_test(TESTCASES, (testcase, element) => {
  let id = element.id;
  return [
    new Promise(resolve => {
      element.addEventListener(
        "input",
        () => {
          Assert.ok(true, "Checking " + id + " field fires input event");
          resolve();
        },
        { once: true }
      );
    }),
    new Promise(resolve => {
      element.addEventListener(
        "change",
        () => {
          Assert.ok(true, "Checking " + id + " field fires change event");
          Assert.equal(
            element.value,
            testcase.expectedResult[id],
            "Check the " + id + " field was filled with correct data"
          );
          resolve();
        },
        { once: true }
      );
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
        Assert.equal(
          element.value,
          testcase.expectedResult[id],
          "Check no value is changed on the " + id + " field"
        );
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
      element.addEventListener(
        "input",
        () => {
          Assert.equal(
            element.value,
            testcase.expectedResult[id],
            "Check the " + id + " field was filled with correct data"
          );
          resolve();
        },
        { once: true }
      );
    }),
  ];
});
