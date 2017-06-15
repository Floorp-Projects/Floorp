"use strict";

Cu.import("resource://formautofill/FormAutofillContent.jsm");

const TESTCASES = [
  {
    description: "Form containing 5 fields with autocomplete attribute.",
    document: `<form id="form1">
                 <input id="street-addr" autocomplete="street-address">
                 <input id="city" autocomplete="address-level2">
                 <select id="country" autocomplete="country"></select>
                 <input id="email" autocomplete="email">
                 <input id="tel" autocomplete="tel">
               </form>`,
    targetInput: ["street-addr", "email"],
    expectedResult: [{
      input: {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
      formId: "form1",
      form: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
      ],
    },
    {
      input: {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      formId: "form1",
      form: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
      ],
    }],
  },
  {
    description: "2 forms that are able to be auto filled",
    document: `<form id="form2">
                 <input id="home-addr" autocomplete="street-address">
                 <input id="city" autocomplete="address-level2">
                 <select id="country" autocomplete="country"></select>
               </form>
               <form id="form3">
                 <input id="office-addr" autocomplete="street-address">
                 <input id="email" autocomplete="email">
                 <input id="tel" autocomplete="tel">
               </form>`,
    targetInput: ["home-addr", "office-addr"],
    expectedResult: [{
      input: {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
      formId: "form2",
      form: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
      ],
    },
    {
      input: {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
      formId: "form3",
      form: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
      ],
    }],
  },
];

function inputDetailAssertion(detail, expected) {
  Assert.equal(detail.section, expected.section);
  Assert.equal(detail.addressType, expected.addressType);
  Assert.equal(detail.contactType, expected.contactType);
  Assert.equal(detail.fieldName, expected.fieldName);
  Assert.equal(detail.elementWeakRef.get(), expected.elementWeakRef.get());
}

TESTCASES.forEach(testcase => {
  add_task(function* () {
    do_print("Starting testcase: " + testcase.description);

    let doc = MockDocument.createTestDocument(
              "http://localhost:8080/test/", testcase.document);

    for (let i in testcase.targetInput) {
      let input = doc.getElementById(testcase.targetInput[i]);
      FormAutofillContent.identifyAutofillFields(input);

      // Put the input element reference to `element` to make sure the result of
      // `getInputDetails` contains the same input element.
      testcase.expectedResult[i].input.elementWeakRef = Cu.getWeakReference(input);

      inputDetailAssertion(FormAutofillContent.getInputDetails(input),
                           testcase.expectedResult[i].input);

      let formDetails = testcase.expectedResult[i].form;
      for (let formDetail of formDetails) {
        // Compose a query string to get the exact reference of <input>/<select>
        // element, e.g. #form1 > *[autocomplete="street-address"]
        let queryString = "#" + testcase.expectedResult[i].formId + " > *[autocomplete=" + formDetail.fieldName + "]";
        formDetail.elementWeakRef = Cu.getWeakReference(doc.querySelector(queryString));
      }

      FormAutofillContent.getFormDetails(input).forEach((detail, index) => {
        inputDetailAssertion(detail, formDetails[index]);
      });
    }
  });
});
