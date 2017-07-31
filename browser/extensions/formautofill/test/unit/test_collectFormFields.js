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
               <input id='email'><input id="phone"></form>`,
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
    validFieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
    ],
    ids: ["given-name", "family-name", "street-addr", "city", "country", "email", "phone"],
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
    validFieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
    ],
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
    validFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},
    ],
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
    validFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},
    ],
  },
  {
    description: "It's a valid address and credit card form.",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-addr" autocomplete="shipping street-address">
               <input id="cc-number" autocomplete="shipping cc-number">
               </form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
    ],
    creditCardFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "cc-number"},
    ],
    validFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "cc-number"},
    ],
  },
  {
    description: "It's an invalid address and credit form.",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input autocomplete="shipping address-level2">
               <input id="cc-name" autocomplete="cc-name">
               <input id="cc-exp-month" autocomplete="cc-exp-month">
               <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    addressFieldDetails: [],
    creditCardFieldDetails: [],
    validFieldDetails: [],
  },
  {
    description: "Three sets of adjacent phone number fields",
    document: `<form>
                 <input id="shippingAreaCode" autocomplete="shipping tel" maxlength="3">
                 <input id="shippingPrefix" autocomplete="shipping tel" maxlength="3">
                 <input id="shippingSuffix" autocomplete="shipping tel" maxlength="4">
                 <input id="shippingTelExt" autocomplete="shipping tel-extension">

                 <input id="billingAreaCode" autocomplete="billing tel" maxlength="3">
                 <input id="billingPrefix" autocomplete="billing tel" maxlength="3">
                 <input id="billingSuffix" autocomplete="billing tel" maxlength="4">

                 <input id="otherCountryCode" autocomplete="tel" maxlength="3">
                 <input id="otherAreaCode" autocomplete="tel" maxlength="3">
                 <input id="otherPrefix" autocomplete="tel" maxlength="3">
                 <input id="otherSuffix" autocomplete="tel" maxlength="4">
               </form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-area-code"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-local-prefix"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-local-suffix"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-extension"},
      {"section": "", "addressType": "billing", "contactType": "", "fieldName": "tel-area-code"},
      {"section": "", "addressType": "billing", "contactType": "", "fieldName": "tel-local-prefix"},
      {"section": "", "addressType": "billing", "contactType": "", "fieldName": "tel-local-suffix"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-country-code"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-area-code"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-prefix"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-suffix"},
    ],
    creditCardFieldDetails: [],
    validFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-area-code"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-local-prefix"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-local-suffix"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-extension"},
      {"section": "", "addressType": "billing", "contactType": "", "fieldName": "tel-area-code"},
      {"section": "", "addressType": "billing", "contactType": "", "fieldName": "tel-local-prefix"},
      {"section": "", "addressType": "billing", "contactType": "", "fieldName": "tel-local-suffix"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-country-code"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-area-code"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-prefix"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-suffix"},
    ],
    ids: [
      "shippingAreaCode", "shippingPrefix", "shippingSuffix", "shippingTelExt",
      "billingAreaCode", "billingPrefix", "billingSuffix",
      "otherCountryCode", "otherAreaCode", "otherPrefix", "otherSuffix",
    ],
  },
  {
    description: "Dedup the same field names of the different telephone fields.",
    document: `<form>
                 <input id="i1" autocomplete="shipping given-name">
                 <input id="i2" autocomplete="shipping family-name">
                 <input id="i3" autocomplete="shipping street-address">
                 <input id="i4" autocomplete="shipping email">

                 <input id="homePhone" maxlength="10">
                 <input id="mobilePhone" maxlength="10">
                 <input id="officePhone" maxlength="10">
               </form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
    ],
    creditCardFieldDetails: [],
    validFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
    ],
    ids: ["i1", "i2", "i3", "i4", "homePhone"],
  },
  {
    description: "The duplicated phones of a single one and a set with ac, prefix, suffix.",
    document: `<form>
                 <input id="i1" autocomplete="shipping given-name">
                 <input id="i2" autocomplete="shipping family-name">
                 <input id="i3" autocomplete="shipping street-address">
                 <input id="i4" autocomplete="shipping email">
                 <input id="singlePhone" autocomplete="shipping tel">
                 <input id="shippingAreaCode" autocomplete="shipping tel-area-code">
                 <input id="shippingPrefix" autocomplete="shipping tel-local-prefix">
                 <input id="shippingSuffix" autocomplete="shipping tel-local-suffix">
               </form>`,
    addressFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},

      // NOTES: Ideally, there is only one full telephone field(s) in a form for
      // this case. We can see if there is any better solution later.
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},

      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-area-code"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-local-prefix"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-local-suffix"},
    ],
    creditCardFieldDetails: [],
    validFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-area-code"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-local-prefix"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-local-suffix"},
    ],
    ids: ["i1", "i2", "i3", "i4", "singlePhone",
      "shippingAreaCode", "shippingPrefix", "shippingSuffix"],
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

      function setElementWeakRef(details) {
        if (!details) {
          return;
        }

        details.forEach((detail, index) => {
          let elementRef;
          if (testcase.ids && testcase.ids[index]) {
            elementRef = doc.getElementById(testcase.ids[index]);
          } else {
            elementRef = doc.querySelector("*[autocomplete*='" + detail.fieldName + "']");
          }
          detail.elementWeakRef = Cu.getWeakReference(elementRef);
        });
      }

      function verifyDetails(handlerDetails, testCaseDetails) {
        if (handlerDetails === null) {
          Assert.equal(handlerDetails, testCaseDetails);
          return;
        }
        Assert.equal(handlerDetails.length, testCaseDetails.length);
        handlerDetails.forEach((detail, index) => {
          Assert.equal(detail.fieldName, testCaseDetails[index].fieldName, "fieldName");
          Assert.equal(detail.section, testCaseDetails[index].section, "section");
          Assert.equal(detail.addressType, testCaseDetails[index].addressType, "addressType");
          Assert.equal(detail.contactType, testCaseDetails[index].contactType, "contactType");
          Assert.equal(detail.elementWeakRef.get(), testCaseDetails[index].elementWeakRef.get(), "DOM reference");
        });
      }
      [
        testcase.addressFieldDetails,
        testcase.creditCardFieldDetails,
        testcase.validFieldDetails,
      ].forEach(details => setElementWeakRef(details));

      let handler = new FormAutofillHandler(formLike);
      let validFieldDetails = handler.collectFormFields();

      verifyDetails(handler.address.fieldDetails, testcase.addressFieldDetails);
      verifyDetails(handler.creditCard.fieldDetails, testcase.creditCardFieldDetails);
      verifyDetails(validFieldDetails, testcase.validFieldDetails);
    });
  })();
}
