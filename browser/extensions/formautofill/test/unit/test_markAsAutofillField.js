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
  {
    description: "Form containing credit card autocomplete attributes.",
    document: `<form>
                <input id="cc-number" autocomplete="cc-number">
                <input id="cc-name" autocomplete="cc-name">
                <input id="cc-exp-month" autocomplete="cc-exp-month">
                <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    targetElementId: "cc-number",
    expectedResult: ["cc-number", "cc-name", "cc-exp-month", "cc-exp-year"],
  },
  {
    description:
      "Form containing multiple cc-number fields without autocomplete attributes.",
    document: `<form>
                <input id="cc-number1" maxlength="4">
                <input id="cc-number2" maxlength="4">
                <input id="cc-number3" maxlength="4">
                <input id="cc-number4" maxlength="4">
                <input id="cc-name">
                <input id="cc-exp-month">
                <input id="cc-exp-year">
               </form>`,
    targetElementId: "cc-number1",
    expectedResult: [
      "cc-number1",
      "cc-number2",
      "cc-number3",
      "cc-number4",
      "cc-name",
      "cc-exp-month",
      "cc-exp-year",
    ],
  },
  {
    description:
      "Invalid form containing three consecutive cc-number fields without autocomplete attributes.",
    document: `<form>
                <input id="cc-number1" maxlength="4">
                <input id="cc-number2" maxlength="4">
                <input id="cc-number3" maxlength="4">
               </form>`,
    targetElementId: "cc-number1",
    expectedResult: [],
    prefs: [
      ["extensions.formautofill.creditCards.heuristics.testConfidence", "1.0"],
    ],
  },
  {
    description:
      "Invalid form containing five consecutive cc-number fields without autocomplete attributes.",
    document: `<form>
                <input id="cc-number1" maxlength="4">
                <input id="cc-number2" maxlength="4">
                <input id="cc-number3" maxlength="4">
                <input id="cc-number4" maxlength="4">
                <input id="cc-number5" maxlength="4">
               </form>`,
    targetElementId: "cc-number1",
    expectedResult: [],
    prefs: [
      ["extensions.formautofill.creditCards.heuristics.testConfidence", "1.0"],
    ],
  },
  {
    description:
      "Valid form containing three consecutive cc-number fields without autocomplete attributes.",
    document: `<form>
                <input id="cc-number1" maxlength="4">
                <input id="cc-number2" maxlength="4">
                <input id="cc-number3" maxlength="4">
                <input id="cc-name">
                <input id="cc-exp-month">
                <input id="cc-exp-year">
               </form>`,
    targetElementId: "cc-number1",
    expectedResult: ["cc-number3", "cc-name", "cc-exp-month", "cc-exp-year"],
    prefs: [
      ["extensions.formautofill.creditCards.heuristics.testConfidence", "1.0"],
    ],
  },
  {
    description:
      "Valid form containing five consecutive cc-number fields without autocomplete attributes.",
    document: `<form>
                <input id="cc-number1" maxlength="4">
                <input id="cc-number2" maxlength="4">
                <input id="cc-number3" maxlength="4">
                <input id="cc-number4" maxlength="4">
                <input id="cc-number5" maxlength="4">
                <input id="cc-name">
                <input id="cc-exp-month">
                <input id="cc-exp-year">
               </form>`,
    targetElementId: "cc-number1",
    expectedResult: ["cc-number5", "cc-name", "cc-exp-month", "cc-exp-year"],
  },
];

let markedFieldId = [];

var FormAutofillContent;
add_setup(async () => {
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

    if (testcase.prefs) {
      testcase.prefs.forEach(pref => SetPref(pref[0], pref[1]));
    }

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

    if (testcase.prefs) {
      testcase.prefs.forEach(pref => Services.prefs.clearUserPref(pref[0]));
    }
  });
});
