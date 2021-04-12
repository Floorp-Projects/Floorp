"use strict";

const TESTCASES = [
  {
    description: "Form containing 8 fields with autocomplete attribute.",
    document: `<form>
                 <input id="given-name" autocomplete="given-name">
                 <input id="additional-name" autocomplete="additional-name">
                 <input id="family-name" autocomplete="family-name">
                 <input id="street-addr" autocomplete="street-address">
                 <input id="city" autocomplete="address-level2">
                 <input id="country" autocomplete="country">
                 <input id="email" autocomplete="email">
                 <input id="tel" autocomplete="tel">
                 <input id="without-autocomplete-1">
                 <input id="without-autocomplete-2">
               </form>`,
    targetElementId: "given-name",
    expectedResult: [
      "given-name",
      "additional-name",
      "family-name",
      "street-addr",
      "city",
      "country",
      "email",
      "tel",
    ],
  },
  {
    description: "Form containing only 2 fields with autocomplete attribute.",
    document: `<form>
                 <input id="street-addr" autocomplete="street-address">
                 <input id="city" autocomplete="address-level2">
                 <input id="without-autocomplete-1">
                 <input id="without-autocomplete-2">
               </form>`,
    targetElementId: "street-addr",
    expectedResult: [],
  },
  {
    description: "Fields without form element.",
    document: `<input id="street-addr" autocomplete="street-address">
               <input id="city" autocomplete="address-level2">
               <input id="country" autocomplete="country">
               <input id="email" autocomplete="email">
               <input id="tel" autocomplete="tel">
               <input id="without-autocomplete-1">
               <input id="without-autocomplete-2">`,
    targetElementId: "street-addr",
    expectedResult: ["street-addr", "city", "country", "email", "tel"],
  },
];

let markedFieldId = [];

var FormAutofillContent;
add_task(async function setup() {
  ({ FormAutofillContent } = ChromeUtils.import(
    "resource://autofill/FormAutofillContent.jsm"
  ));

  FormAutofillContent._markAsAutofillField = function(field) {
    markedFieldId.push(field.id);
  };
});

TESTCASES.forEach(testcase => {
  add_task(async function() {
    info("Starting testcase: " + testcase.description);

    markedFieldId = [];

    let doc = MockDocument.createTestDocument(
      "http://localhost:8080/test/",
      testcase.document
    );
    let element = doc.getElementById(testcase.targetElementId);
    FormAutofillContent.identifyAutofillFields(element);

    Assert.deepEqual(
      markedFieldId,
      testcase.expectedResult,
      "Check the fields were marked correctly."
    );
  });
});
