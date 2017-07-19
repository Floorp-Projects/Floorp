/*
 * Test for form auto fill content helper collectFormFields functions.
 */

"use strict";

Cu.import("resource://formautofill/FormAutofillHandler.jsm");

const TESTCASES = [
  {
    description: "Form without autocomplete property",
    document: `<form><input id="given-name"><input id="family-name">
               <input id="street-addr"><input id="city"><select id="country"></select>
               <input id='email'><input id="tel"></form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
    ],
    creditCardFieldDetails: [],
    isValidForm: {
      address: true,
      creditCard: false,
    },
    ids: ["given-name", "family-name", "street-addr", "city", "country", "email", "tel"],
  },
  {
    description: "An address and credit card form with autocomplete properties and 1 token",
    document: `<form>
               <input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <input id="street-addr" autocomplete="street-address">
               <input id="city" autocomplete="address-level2">
               <select id="country" autocomplete="country"></select>
               <input id="email" autocomplete="email">
               <input id="tel" autocomplete="tel">
               <input id="cc-number" autocomplete="cc-number">
               <input id="cc-name" autocomplete="cc-name">
               <input id="cc-exp-month" autocomplete="cc-exp-month">
               <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
    ],
    creditCardFieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
    ],
    isValidForm: {
      address: true,
      creditCard: true,
    },
  },
  {
    description: "An address form with autocomplete properties and 2 tokens",
    document: `<form><input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-addr" autocomplete="shipping street-address">
               <input id="city" autocomplete="shipping address-level2">
               <input id="country" autocomplete="shipping country">
               <input id='email' autocomplete="shipping email">
               <input id="tel" autocomplete="shipping tel"></form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},
    ],
    creditCardFieldDetails: [],
    isValidForm: {
      address: true,
      creditCard: false,
    },
  },
  {
    description: "Form with autocomplete properties and profile is partly matched",
    document: `<form><input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-addr" autocomplete="shipping street-address">
               <input autocomplete="shipping address-level2">
               <select autocomplete="shipping country"></select>
               <input id='email' autocomplete="shipping email">
               <input id="tel" autocomplete="shipping tel"></form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},
    ],
    creditCardFieldDetails: [],
    isValidForm: {
      address: true,
      creditCard: false,
    },
  },
  {
    description: "It's a valid address and credit card form.",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-addr" autocomplete="shipping street-address">
               <input id="city" autocomplete="shipping address-level2">
               <input id="cc-number" autocomplete="shipping cc-number">
               </form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2"},
    ],
    creditCardFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "cc-number"},
    ],
    isValidForm: {
      address: true,
      creditCard: true,
    },
  },
  {
    description: "It's an invalid address and credit form.",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input autocomplete="shipping address-level2">
               <select autocomplete="shipping country"></select>
               <input id="cc-name" autocomplete="cc-name">
               <input id="cc-exp-month" autocomplete="cc-exp-month">
               <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country"},
    ],
    creditCardFieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
    ],
    isValidForm: {
      address: false,
      creditCard: false,
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
      let formLike = FormLikeFactory.createFromForm(form);

      Array.of(
        ...testcase.addressFieldDetails,
        ...testcase.creditCardFieldDetails
      ).forEach((detail, index) => {
        let elementRef;
        if (testcase.ids && testcase.ids[index]) {
          elementRef = doc.getElementById(testcase.ids[index]);
        } else {
          elementRef = doc.querySelector("*[autocomplete*='" + detail.fieldName + "']");
        }
        detail.elementWeakRef = Cu.getWeakReference(elementRef);
      });
      let handler = new FormAutofillHandler(formLike);

      handler.collectFormFields();

      function verifyDetails(handlerDetails, testCaseDetails) {
        handlerDetails.forEach((detail, index) => {
          Assert.equal(detail.section, testCaseDetails[index].section);
          Assert.equal(detail.addressType, testCaseDetails[index].addressType);
          Assert.equal(detail.contactType, testCaseDetails[index].contactType);
          Assert.equal(detail.fieldName, testCaseDetails[index].fieldName);
          Assert.equal(detail.elementWeakRef.get(), testCaseDetails[index].elementWeakRef.get());
        });
      }

      verifyDetails(handler.addressFieldDetails, testcase.addressFieldDetails);
      verifyDetails(handler.creditCardFieldDetails, testcase.creditCardFieldDetails);

      Assert.equal(handler.isValidAddressForm, testcase.isValidForm.address, "Valid Address Form Checking");
      Assert.equal(handler.isValidCreditCardForm, testcase.isValidForm.creditCard, "Valid Credit Card Form Checking");
    });
  })();
}
