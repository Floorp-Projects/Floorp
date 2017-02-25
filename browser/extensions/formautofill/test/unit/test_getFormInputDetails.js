"use strict";

Cu.import("resource://formautofill/FormAutofillContent.jsm");

const TESTCASES = [
  {
    description: "Form containing 5 fields with autocomplete attribute.",
    document: `<form id="form1">
                 <input id="street-addr" autocomplete="street-address">
                 <input id="city" autocomplete="address-level2">
                 <input id="country" autocomplete="country">
                 <input id="email" autocomplete="email">
                 <input id="tel" autocomplete="tel">
               </form>`,
    targetInput: ["street-addr", "country"],
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
      input: {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
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
                 <input id="country" autocomplete="country">
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


TESTCASES.forEach(testcase => {
  add_task(function* () {
    do_print("Starting testcase: " + testcase.description);

    let doc = MockDocument.createTestDocument(
              "http://localhost:8080/test/", testcase.document);
    FormAutofillContent.identifyAutofillFields(doc);

    for (let i in testcase.targetInput) {
      let input = doc.getElementById(testcase.targetInput[i]);

      // Put the input element reference to `element` to make sure the result of
      // `getInputDetails` contains the same input element.
      testcase.expectedResult[i].input.element = input;
      Assert.deepEqual(FormAutofillContent.getInputDetails(input),
                       testcase.expectedResult[i].input,
                       "Check if returned input information is correct.");

      let formDetails = testcase.expectedResult[i].form;
      for (let formDetail of formDetails) {
        // Compose a query string to get the exact reference of the input
        // element, e.g. #form1 > input[autocomplete="street-address"]
        let queryString = "#" + testcase.expectedResult[i].formId + " > input[autocomplete=" + formDetail.fieldName + "]";
        formDetail.element = doc.querySelector(queryString);
      }
      Assert.deepEqual(FormAutofillContent.getFormDetails(input),
                       formDetails,
                       "Check if returned form information is correct.");
    }
  });
});
