/*
 * Test for the normalization of records created by FormAutofillHandler.
 */

"use strict";

var FormAutofillHandler;
add_task(async function seutp() {
  ({ FormAutofillHandler } = ChromeUtils.importESModule(
    "resource://gre/modules/shared/FormAutofillHandler.sys.mjs"
  ));
});

const TESTCASES = [
  {
    description:
      "Don't contain a field whose length of value is greater than 200",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="organization" autocomplete="organization">
                <input id="address-level1" autocomplete="address-level1">
                <input id="country" autocomplete="country">
                <input id="cc-number" autocomplete="cc-number">
                <input id="cc-name" autocomplete="cc-name">
               </form>`,
    formValue: {
      "given-name": "John",
      organization: "*".repeat(200),
      "address-level1": "*".repeat(201),
      country: "US",
      "cc-number": "1111222233334444",
      "cc-name": "*".repeat(201),
    },
    expectedRecord: {
      address: [
        {
          "given-name": "John",
          organization: "*".repeat(200),
          "address-level1": "",
          country: "US",
        },
      ],
      creditCard: [
        {
          "cc-number": "1111222233334444",
          "cc-name": "",
        },
      ],
    },
  },
  {
    description: "Don't create address record if filled data is less than 3",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="organization" autocomplete="organization">
                <input id="country" autocomplete="country">
               </form>`,
    formValue: {
      "given-name": "John",
      organization: "Mozilla",
    },
    expectedRecord: {
      address: [],
      creditCard: [],
    },
  },
  {
    description: "All name related fields should be counted as 1 field only.",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="family-name" autocomplete="family-name">
                <input id="organization" autocomplete="organization">
               </form>`,
    formValue: {
      "given-name": "John",
      "family-name": "Doe",
      organization: "Mozilla",
    },
    expectedRecord: {
      address: [],
      creditCard: [],
    },
  },
  {
    description:
      "All telephone related fields should be counted as 1 field only.",
    document: `<form>
                <input id="tel-country-code" autocomplete="tel-country-code">
                <input id="tel-area-code" autocomplete="tel-area-code">
                <input id="tel-local" autocomplete="tel-local">
                <input id="organization" autocomplete="organization">
               </form>`,
    formValue: {
      "tel-country-code": "+1",
      "tel-area-code": "123",
      "tel-local": "4567890",
      organization: "Mozilla",
    },
    expectedRecord: {
      address: [],
      creditCard: [],
    },
  },
  {
    description:
      "A credit card form with the value of cc-number, cc-exp, cc-name and cc-type.",
    document: `<form>
                <input id="cc-number" autocomplete="cc-number">
                <input id="cc-name" autocomplete="cc-name">
                <input id="cc-exp" autocomplete="cc-exp">
                <input id="cc-type" autocomplete="cc-type">
               </form>`,
    formValue: {
      "cc-number": "5105105105105100",
      "cc-name": "Foo Bar",
      "cc-exp": "2022-06",
      "cc-type": "Visa",
    },
    expectedRecord: {
      address: [],
      creditCard: [
        {
          "cc-number": "5105105105105100",
          "cc-name": "Foo Bar",
          "cc-exp": "2022-06",
          "cc-type": "Visa",
        },
      ],
    },
  },
  {
    description: "A credit card form with cc-number value only.",
    document: `<form>
                <input id="cc-number" autocomplete="cc-number">
               </form>`,
    formValue: {
      "cc-number": "4111111111111111",
    },
    expectedRecord: {
      address: [],
      creditCard: [
        {
          "cc-number": "4111111111111111",
        },
      ],
    },
  },
  {
    description: "A credit card form must have cc-number value.",
    document: `<form>
                <input id="cc-number" autocomplete="cc-number">
                <input id="cc-name" autocomplete="cc-name">
                <input id="cc-exp" autocomplete="cc-exp">
               </form>`,
    formValue: {
      "cc-number": "",
      "cc-name": "Foo Bar",
      "cc-exp": "2022-06",
    },
    expectedRecord: {
      address: [],
      creditCard: [],
    },
  },
  {
    description: "A credit card form must have cc-number field.",
    document: `<form>
                <input id="cc-name" autocomplete="cc-name">
                <input id="cc-exp" autocomplete="cc-exp">
               </form>`,
    formValue: {
      "cc-name": "Foo Bar",
      "cc-exp": "2022-06",
    },
    expectedRecord: {
      address: [],
      creditCard: [],
    },
  },
  {
    description: "A form with multiple sections",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="organization" autocomplete="organization">
                <input id="country" autocomplete="country">

                <input id="given-name-shipping" autocomplete="shipping given-name">
                <input id="family-name-shipping" autocomplete="shipping family-name">
                <input id="organization-shipping" autocomplete="shipping organization">
                <input id="country-shipping" autocomplete="shipping country">

                <input id="given-name-billing" autocomplete="billing given-name">
                <input id="organization-billing" autocomplete="billing organization">
                <input id="country-billing" autocomplete="billing country">

                <input id="cc-number-section-one" autocomplete="section-one cc-number">
                <input id="cc-name-section-one" autocomplete="section-one cc-name">

                <input id="cc-number-section-two" autocomplete="section-two cc-number">
                <input id="cc-name-section-two" autocomplete="section-two cc-name">
                <input id="cc-exp-section-two" autocomplete="section-two cc-exp">
               </form>`,
    formValue: {
      "given-name": "Bar",
      organization: "Foo",
      country: "US",

      "given-name-shipping": "John",
      "family-name-shipping": "Doe",
      "organization-shipping": "Mozilla",
      "country-shipping": "US",

      "given-name-billing": "Foo",
      "organization-billing": "Bar",
      "country-billing": "US",

      "cc-number-section-one": "4111111111111111",
      "cc-name-section-one": "John",

      "cc-number-section-two": "5105105105105100",
      "cc-name-section-two": "Foo Bar",
      "cc-exp-section-two": "2026-26",
    },
    expectedRecord: {
      address: [
        {
          "given-name": "Bar",
          organization: "Foo",
          country: "US",
        },
        {
          "given-name": "John",
          "family-name": "Doe",
          organization: "Mozilla",
          country: "US",
        },
        {
          "given-name": "Foo",
          organization: "Bar",
          country: "US",
        },
      ],
      creditCard: [
        {
          "cc-number": "4111111111111111",
          "cc-name": "John",
        },
        {
          "cc-number": "5105105105105100",
          "cc-name": "Foo Bar",
          "cc-exp": "2026-26",
        },
      ],
    },
  },
  {
    description: "A credit card form with a cc-type select.",
    document: `<form>
                <input id="cc-number" autocomplete="cc-number">
                <label for="field1">Card Type:</label>
                <select id="field1">
                  <option value="visa" selected>Visa</option>
                </select>
               </form>`,
    formValue: {
      "cc-number": "5105105105105100",
    },
    expectedRecord: {
      address: [],
      creditCard: [
        {
          "cc-number": "5105105105105100",
          "cc-type": "visa",
        },
      ],
    },
  },
  {
    description: "A credit card form with a cc-type select from label.",
    document: `<form>
                <input id="cc-number" autocomplete="cc-number">
                <label for="cc-type">Card Type:</label>
                <select id="cc-type">
                  <option value="V" selected>Visa</option>
                  <option value="A">American Express</option>
                </select>
               </form>`,
    formValue: {
      "cc-number": "5105105105105100",
      "cc-type": "A",
    },
    expectedRecord: {
      address: [],
      creditCard: [
        {
          "cc-number": "5105105105105100",
          "cc-type": "amex",
        },
      ],
    },
  },
  {
    description:
      "A credit card form with separate expiry fields should have normalized expiry data.",
    document: `<form>
                <input id="cc-number" autocomplete="cc-number">
                <input id="cc-exp-month" autocomplete="cc-exp-month">
                <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    formValue: {
      "cc-number": "5105105105105100",
      "cc-exp-month": "05",
      "cc-exp-year": "26",
    },
    expectedRecord: {
      address: [],
      creditCard: [
        {
          "cc-number": "5105105105105100",
          "cc-exp-month": "05",
          "cc-exp-year": "26",
        },
      ],
    },
  },
  {
    description:
      "A credit card form with combined expiry fields should have normalized expiry data.",
    document: `<form>
                <input id="cc-number" autocomplete="cc-number">
                <input id="cc-exp" autocomplete="cc-exp">
               </form>`,
    formValue: {
      "cc-number": "5105105105105100",
      "cc-exp": "07/27",
    },
    expectedRecord: {
      address: [],
      creditCard: [
        {
          "cc-number": "5105105105105100",
          "cc-exp": "07/27",
        },
      ],
    },
  },
];

for (let testcase of TESTCASES) {
  add_task(async function () {
    info("Starting testcase: " + testcase.description);

    let doc = MockDocument.createTestDocument(
      "http://localhost:8080/test/",
      testcase.document
    );
    let form = doc.querySelector("form");
    let formLike = FormLikeFactory.createFromForm(form);
    let handler = new FormAutofillHandler(formLike);

    handler.collectFormFields();

    for (let id in testcase.formValue) {
      doc.getElementById(id).value = testcase.formValue[id];
    }

    let record = handler.createRecords();

    let expectedRecord = testcase.expectedRecord;
    for (let type in record) {
      Assert.deepEqual(
        record[type].map(secRecord => secRecord.record),
        expectedRecord[type]
      );
    }
  });
}
