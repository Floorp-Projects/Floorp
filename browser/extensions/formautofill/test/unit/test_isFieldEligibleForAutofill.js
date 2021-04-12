"use strict";

var FormAutofillUtils;
add_task(async function seutp() {
  ({ FormAutofillUtils } = ChromeUtils.import(
    "resource://autofill/FormAutofillUtils.jsm"
  ));
});

const TESTCASES = [
  {
    document: `<input id="targetElement" type="text">`,
    fieldId: "targetElement",
    expectedResult: true,
  },
  {
    document: `<input id="targetElement" type="email">`,
    fieldId: "targetElement",
    expectedResult: true,
  },
  {
    document: `<input id="targetElement" type="number">`,
    fieldId: "targetElement",
    expectedResult: true,
  },
  {
    document: `<input id="targetElement" type="tel">`,
    fieldId: "targetElement",
    expectedResult: true,
  },
  {
    document: `<input id="targetElement" type="radio">`,
    fieldId: "targetElement",
    expectedResult: false,
  },
  {
    document: `<input id="targetElement" type="text" autocomplete="off">`,
    fieldId: "targetElement",
    expectedResult: true,
  },
  {
    document: `<input id="targetElement">`,
    fieldId: "targetElement",
    expectedResult: true,
  },
  {
    document: `<input id="targetElement" type="unknown">`,
    fieldId: "targetElement",
    expectedResult: true,
  },
  {
    document: `<select id="targetElement" autocomplete="off"></select>`,
    fieldId: "targetElement",
    expectedResult: true,
  },
  {
    document: `<select id="targetElement"></select>`,
    fieldId: "targetElement",
    expectedResult: true,
  },
  {
    document: `<select id="targetElement" multiple></select>`,
    fieldId: "targetElement",
    expectedResult: true,
  },
  {
    document: `<div id="targetElement"></div>`,
    fieldId: "targetElement",
    expectedResult: false,
  },
];

TESTCASES.forEach(testcase => {
  add_task(async function() {
    info("Starting testcase: " + testcase.document);

    let doc = MockDocument.createTestDocument(
      "http://localhost:8080/test/",
      testcase.document
    );

    let field = doc.getElementById(testcase.fieldId);
    Assert.equal(
      FormAutofillUtils.isFieldEligibleForAutofill(field),
      testcase.expectedResult
    );
  });
});
