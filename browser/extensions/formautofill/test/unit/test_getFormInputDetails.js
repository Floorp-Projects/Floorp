"use strict";

Cu.import("resource://formautofill/FormAutofillContent.jsm");

const TESTCASES = [
  {
    description: "Form containing 5 fields with autocomplete attribute.",
    document: `<form>
                 <input id="street-addr" autocomplete="street-address">
                 <input id="city" autocomplete="address-level2">
                 <input id="country" autocomplete="country">
                 <input id="email" autocomplete="email">
                 <input id="tel" autocomplete="tel">
               </form>`,
    targetInput: ["street-addr", "country"],
    expectedResult: [{
      input: {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
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
    document: `<form>
                 <input id="home-addr" autocomplete="street-address">
                 <input id="city" autocomplete="address-level2">
                 <input id="country" autocomplete="country">
               </form>
               <form>
                 <input id="office-addr" autocomplete="street-address">
                 <input id="email" autocomplete="email">
                 <input id="tel" autocomplete="tel">
               </form>`,
    targetInput: ["home-addr", "office-addr"],
    expectedResult: [{
      input: {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
      form: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
      ],
    },
    {
      input: {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
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
    FormAutofillContent._identifyAutofillFields(doc);

    for (let i in testcase.targetInput) {
      let input = doc.getElementById(testcase.targetInput[i]);

      Assert.deepEqual(FormAutofillContent.getInputDetails(input),
                       testcase.expectedResult[i].input,
                       "Check if returned input information is correct.");

      Assert.deepEqual(FormAutofillContent.getFormDetails(input),
                       testcase.expectedResult[i].form,
                       "Check if returned form information is correct.");
    }
  });
});
