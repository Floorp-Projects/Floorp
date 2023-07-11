/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Bug 1762063 - we need to fix this pattern of having to wrap destructuring calls in parentheses.
// We can't do a standard destructuring call because FormAutofillUtils is already declared as a var in head.js
({ FormAutofillUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/shared/FormAutofillUtils.sys.mjs"
));
const { FIELD_STATES } = FormAutofillUtils;
const PREVIEW = FIELD_STATES.PREVIEW;
const NORMAL = FIELD_STATES.NORMAL;

const { OSKeyStore } = ChromeUtils.importESModule(
  "resource://gre/modules/OSKeyStore.sys.mjs"
);

const TESTCASES = [
  {
    description: "Preview best case address form",
    document: `<form>
    <input id="given-name" autocomplete="given-name">
    <input id="family-name" autocomplete="family-name">
    <input id="street-addr" autocomplete="street-address">
    <input id="city" autocomplete="address-level2">
   </form>`,
    focusedInputId: "given-name",
    profileData: {
      "given-name": "John",
      "family-name": "Doe",
      "street-address": "100 Main Street",
      "address-level2": "Hamilton",
    },
    expectedResultState: {
      "given-name": PREVIEW,
      "family-name": PREVIEW,
      "street-address": PREVIEW,
      "address-level2": PREVIEW,
    },
  },
  {
    description: "Preview form with a readonly input and non-readonly inputs",
    document: `<form>
    <input id="given-name" autocomplete="given-name">
    <input id="family-name" autocomplete="family-name">
    <input id="street-addr" autocomplete="street-address">
    <input id="city" autocomplete="address-level2" readonly value="TEST CITY">
   </form>`,
    focusedInputId: "given-name",
    profileData: {
      "given-name": "John",
      "family-name": "Doe",
      "street-address": "100 Main Street",
      city: "Hamilton",
    },
    expectedResultState: {
      "given-name": PREVIEW,
      "family-name": PREVIEW,
      "street-address": PREVIEW,
      "address-level2": undefined,
    },
  },
  {
    description: "Preview form with a disabled input and non-disabled inputs",
    document: `<form>
    <input id="given-name" autocomplete="given-name">
    <input id="family-name" autocomplete="family-name">
    <input id="street-addr" autocomplete="street-address">
    <input id="country" autocomplete="country" disabled value="US">
   </form>`,
    focusedInputId: "given-name",
    profileData: {
      "given-name": "John",
      "family-name": "Doe",
      "street-address": "100 Main Street",
      country: "CA",
    },
    expectedResultState: {
      "given-name": PREVIEW,
      "family-name": PREVIEW,
      "street-address": PREVIEW,
      country: undefined,
    },
  },
  {
    description:
      "Preview form with autocomplete select elements and matching option values",
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
      country: "US",
      "address-level1": "CA",
    },
    expectedResultState: {
      "given-name": NORMAL,
      country: PREVIEW,
      "address-level1": PREVIEW,
    },
  },
  {
    description: "Preview best case credit card form",
    document: `<form>
              <input id="cc-number" autocomplete="cc-number">
              <input id="cc-name" autocomplete="cc-name">
              <input id="cc-exp-month" autocomplete="cc-exp-month">
              <input id="cc-exp-year" autocomplete="cc-exp-year">
              <input id="cc-csc" autocomplete="cc-csc">
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
    expectedResultState: {
      "cc-number": PREVIEW,
      "cc-name": PREVIEW,
      "cc-exp-month": PREVIEW,
      "cc-exp-year": PREVIEW,
      "cc-csc": NORMAL,
    },
  },
];

function run_tests(testcases) {
  for (let testcase of testcases) {
    add_task(async function () {
      info("Starting testcase: " + testcase.description);
      let doc = MockDocument.createTestDocument(
        "http://localhost:8080/test/",
        testcase.document
      );
      let form = doc.querySelector("form");
      let formLike = FormLikeFactory.createFromForm(form);
      let handler = new FormAutofillHandler(formLike);

      // Replace the internal decrypt method with OSKeyStore API,
      // but don't pass the reauth parameter to avoid triggering
      // reauth login dialog in these tests.
      let decryptHelper = async (cipherText, reauth) => {
        return OSKeyStore.decrypt(cipherText, false);
      };
      handler.collectFormFields();

      let focusedInput = doc.getElementById(testcase.focusedInputId);
      try {
        handler.focusedInput = focusedInput;
      } catch (e) {
        if (e.message.includes("WeakMap key must be an object")) {
          throw new Error(
            `Couldn't find the focusedInputId in the current form! Make sure focusedInputId exists in your test form! testcase description:${testcase.description}`
          );
        } else {
          throw e;
        }
      }

      for (let section of handler.sections) {
        section._decrypt = decryptHelper;
      }

      let [adaptedProfile] = handler.activeSection.getAdaptedProfiles([
        testcase.profileData,
      ]);

      await handler.activeSection.previewFormFields(adaptedProfile);

      for (let field of handler.fieldDetails) {
        let actual = handler.getFilledStateByElement(field.element);
        let expected = testcase.expectedResultState[field.fieldName];
        info(`Checking ${field.fieldName} state`);
        Assert.equal(
          actual,
          expected,
          "Check if preview state is set correctly"
        );
      }
    });
  }
}

run_tests(TESTCASES);
