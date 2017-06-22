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
    fieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
    ],
    ids: ["given-name", "family-name", "street-addr", "city", "country", "email", "tel"],
  },
  {
    description: "Form with autocomplete properties and 1 token",
    document: `<form><input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <input id="street-addr" autocomplete="street-address">
               <input id="city" autocomplete="address-level2">
               <select id="country" autocomplete="country"></select>
               <input id="email" autocomplete="email">
               <input id="tel" autocomplete="tel"></form>`,
    fieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
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
    fieldDetails: [
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
    fieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},
    ],
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

      testcase.fieldDetails.forEach((detail, index) => {
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

      handler.fieldDetails.forEach((detail, index) => {
        Assert.equal(detail.section, testcase.fieldDetails[index].section);
        Assert.equal(detail.addressType, testcase.fieldDetails[index].addressType);
        Assert.equal(detail.contactType, testcase.fieldDetails[index].contactType);
        Assert.equal(detail.fieldName, testcase.fieldDetails[index].fieldName);
        Assert.equal(detail.elementWeakRef.get(), testcase.fieldDetails[index].elementWeakRef.get());
      });
    });
  })();
}
