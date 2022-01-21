"use strict";

var FormAutofillHeuristics, LabelUtils;
add_task(async function() {
  ({ FormAutofillHeuristics, LabelUtils } = ChromeUtils.import(
    "resource://autofill/FormAutofillHeuristics.jsm"
  ));
});

const TESTCASES = [
  {
    description: "Input element in a label element",
    document: `<form>
                 <label> E-Mail
                   <input id="targetElement" type="text">
                 </label>
               </form>`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "email",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description:
      "A label element is out of the form contains the related input",
    document: `<label for="targetElement"> E-Mail</label>
               <form>
                 <input id="targetElement" type="text">
               </form>`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "email",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: "A label element contains span element",
    document: `<label for="targetElement">FOO<span>E-Mail</span>BAR</label>
               <form>
                 <input id="targetElement" type="text">
               </form>`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "email",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: "The signature in 'name' attr of an input",
    document: `<input id="targetElement" name="email" type="text">`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "email",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: "The signature in 'id' attr of an input",
    document: `<input id="targetElement_email" name="tel" type="text">`,
    elementId: "targetElement_email",
    expectedReturnValue: {
      fieldName: "email",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: "Select element in a label element",
    document: `<form>
                 <label> State
                   <select id="targetElement"></select>
                 </label>
               </form>`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "address-level1",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: "A select element without a form wrapped",
    document: `<label for="targetElement">State</label>
               <select id="targetElement"></select>`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "address-level1",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: "address line input",
    document: `<label for="targetElement">street</label>
               <input id="targetElement" type="text">`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "street-address",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: "CJK character - Traditional Chinese",
    document: `<label> 郵遞區號
                 <input id="targetElement" />
               </label>`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "postal-code",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: "CJK character - Japanese",
    document: `<label> 郵便番号
                 <input id="targetElement" />
               </label>`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "postal-code",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: "CJK character - Korean",
    document: `<label> 우편 번호
                 <input id="targetElement" />
               </label>`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "postal-code",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: "",
    document: `<input id="targetElement" name="fullname">`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "name",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: 'input element with "submit" type',
    document: `<input id="targetElement" type="submit" />`,
    elementId: "targetElement",
    expectedReturnValue: null,
  },
  {
    description: "The signature in 'name' attr of an email input",
    document: `<input id="targetElement" name="email" type="number">`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "email",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: 'input element with "email" type',
    document: `<input id="targetElement" type="email" />`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "email",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: "Exclude United State string",
    document: `<label>United State
                 <input id="targetElement" />
               </label>`,
    elementId: "targetElement",
    expectedReturnValue: null,
  },
  {
    description: '"County" field with "United State" string',
    document: `<label>United State County
                 <input id="targetElement" />
               </label>`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "address-level1",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: '"city" field with double "United State" string',
    document: `<label>United State united sTATE city
                 <input id="targetElement" />
               </label>`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "address-level2",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: "Verify credit card number",
    document: `<form>
                 <label for="targetElement"> Card Number</label>
                 <input id="targetElement" type="text">
               </form>`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "cc-number",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
  {
    description: "Identify credit card type field",
    document: `<form>
                 <label for="targetElement">Card Type</label>
                 <input id="targetElement" type="text">
               </form>`,
    elementId: "targetElement",
    expectedReturnValue: {
      fieldName: "cc-type",
      section: "",
      addressType: "",
      contactType: "",
    },
  },
];

TESTCASES.forEach(testcase => {
  add_task(async function() {
    info("Starting testcase: " + testcase.description);

    let doc = MockDocument.createTestDocument(
      "http://localhost:8080/test/",
      testcase.document
    );

    let element = doc.getElementById(testcase.elementId);
    let value = FormAutofillHeuristics.getInfo(element);

    Assert.deepEqual(value, testcase.expectedReturnValue);
    LabelUtils.clearLabelMap();
  });
});

add_task(async function test_regexp_list() {
  info("Verify the fieldName support for select element.");
  let SUPPORT_LIST = {
    email: null, // email
    "tel-extension": null, // tel-extension
    phone: null, // tel
    organization: null, // organization
    "street-address": null, // street-address
    address1: null, // address-line1
    address2: null, // address-line2
    address3: null, // address-line3
    city: "address-level2",
    region: "address-level1",
    "postal-code": null, // postal-code
    country: "country",
    fullname: null, // name
    fname: null, // given-name
    mname: null, // additional-name
    lname: null, // family-name
    cardholder: null, // cc-name
    "cc-number": null, // cc-number
    addmonth: "cc-exp-month",
    addyear: "cc-exp-year",
  };
  for (let label of Object.keys(SUPPORT_LIST)) {
    let testcase = {
      description: `A select element supports ${label} or not`,
      document: `<select id="${label}"></select>`,
      elementId: label,
      expectedReturnValue: SUPPORT_LIST[label]
        ? {
            fieldName: SUPPORT_LIST[label],
            section: "",
            addressType: "",
            contactType: "",
          }
        : null,
    };
    info(testcase.description);
    info(testcase.document);
    let doc = MockDocument.createTestDocument(
      "http://localhost:8080/test/",
      testcase.document
    );

    let element = doc.getElementById(testcase.elementId);
    let value = FormAutofillHeuristics.getInfo(element);

    Assert.deepEqual(value, testcase.expectedReturnValue, label);
  }
  LabelUtils.clearLabelMap();
});
