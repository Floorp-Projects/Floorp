/*
 * Test for the normalization of records created by FormAutofillHandler.
 */

"use strict";

Cu.import("resource://formautofill/FormAutofillHandler.jsm");

const TESTCASES = [
  {
    description: "Don't contain a field whose length of value is greater than 200",
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
      "organization": "*".repeat(200),
      "address-level1": "*".repeat(201),
      "country": "US",
      "cc-number": "1111222233334444",
      "cc-name": "*".repeat(201),
    },
    expectedRecord: {
      address: [{
        "given-name": "John",
        "organization": "*".repeat(200),
        "address-level1": "",
        "country": "US",
      }],
      creditCard: [{
        "cc-number": "1111222233334444",
        "cc-name": "",
      }],
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
      "organization": "Mozilla",
    },
    expectedRecord: {
      address: [],
      creditCard: [],
    },
  },
  {
    description: "\"country\" using @autocomplete shouldn't be identified aggressively",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="organization" autocomplete="organization">
                <input id="country" autocomplete="country">
               </form>`,
    formValue: {
      "given-name": "John",
      "organization": "Mozilla",
      "country": "United States",
    },
    expectedRecord: {
      address: [{
        "given-name": "John",
        "organization": "Mozilla",
        "country": "United States",
      }],
      creditCard: [],
    },
  },
  {
    description: "\"country\" using heuristics should be identified aggressively",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="organization" autocomplete="organization">
                <input id="country" name="country">
               </form>`,
    formValue: {
      "given-name": "John",
      "organization": "Mozilla",
      "country": "United States",
    },
    expectedRecord: {
      address: [{
        "given-name": "John",
        "organization": "Mozilla",
        "country": "US",
      }],
      creditCard: [],
    },
  },
  {
    description: "\"tel\" related fields should be concatenated",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="organization" autocomplete="organization">
                <input id="tel-country-code" autocomplete="tel-country-code">
                <input id="tel-national" autocomplete="tel-national">
               </form>`,
    formValue: {
      "given-name": "John",
      "organization": "Mozilla",
      "tel-country-code": "+1",
      "tel-national": "1234567890",
    },
    expectedRecord: {
      address: [{
        "given-name": "John",
        "organization": "Mozilla",
        "tel": "+11234567890",
      }],
      creditCard: [],
    },
  },
  {
    description: "\"tel\" should be removed if it's too short",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="organization" autocomplete="organization">
                <input id="country" autocomplete="country">
                <input id="tel" autocomplete="tel-national">
               </form>`,
    formValue: {
      "given-name": "John",
      "organization": "Mozilla",
      "country": "United States",
      "tel": "1234",
    },
    expectedRecord: {
      address: [{
        "given-name": "John",
        "organization": "Mozilla",
        "country": "United States",
        "tel": "",
      }],
      creditCard: [],
    },
  },
  {
    description: "\"tel\" should be removed if it's too long",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="organization" autocomplete="organization">
                <input id="country" autocomplete="country">
                <input id="tel" autocomplete="tel-national">
               </form>`,
    formValue: {
      "given-name": "John",
      "organization": "Mozilla",
      "country": "United States",
      "tel": "1234567890123456",
    },
    expectedRecord: {
      address: [{
        "given-name": "John",
        "organization": "Mozilla",
        "country": "United States",
        "tel": "",
      }],
      creditCard: [],
    },
  },
  {
    description: "\"tel\" should be removed if it contains invalid characters",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="organization" autocomplete="organization">
                <input id="country" autocomplete="country">
                <input id="tel" autocomplete="tel-national">
               </form>`,
    formValue: {
      "given-name": "John",
      "organization": "Mozilla",
      "country": "United States",
      "tel": "12345###!!!",
    },
    expectedRecord: {
      address: [{
        "given-name": "John",
        "organization": "Mozilla",
        "country": "United States",
        "tel": "",
      }],
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
      "organization": "Mozilla",
    },
    expectedRecord: {
      address: [],
      creditCard: [],
    },
  },
  {
    description: "All telephone related fields should be counted as 1 field only.",
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
      "organization": "Mozilla",
    },
    expectedRecord: {
      address: [],
      creditCard: [],
    },
  },
  {
    description: "A credit card form with the value of cc-number, cc-exp, and cc-name.",
    document: `<form>
                <input id="cc-number" autocomplete="cc-number">
                <input id="cc-name" autocomplete="cc-name">
                <input id="cc-exp" autocomplete="cc-exp">
               </form>`,
    formValue: {
      "cc-number": "4444000022220000",
      "cc-name": "Foo Bar",
      "cc-exp": "2022-06",
    },
    expectedRecord: {
      address: [],
      creditCard: [{
        "cc-number": "4444000022220000",
        "cc-name": "Foo Bar",
        "cc-exp": "2022-06",
      }],
    },
  },
  {
    description: "A credit card form with cc-number value only.",
    document: `<form>
                <input id="cc-number" autocomplete="cc-number">
               </form>`,
    formValue: {
      "cc-number": "4444000022220000",
    },
    expectedRecord: {
      address: [],
      creditCard: [{
        "cc-number": "4444000022220000",
      }],
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
      "organization": "Foo",
      "country": "US",

      "given-name-shipping": "John",
      "family-name-shipping": "Doe",
      "organization-shipping": "Mozilla",
      "country-shipping": "US",

      "given-name-billing": "Foo",
      "organization-billing": "Bar",
      "country-billing": "US",

      "cc-number-section-one": "4444000022220000",
      "cc-name-section-one": "John",

      "cc-number-section-two": "4444000022221111",
      "cc-name-section-two": "Foo Bar",
      "cc-exp-section-two": "2026-26",
    },
    expectedRecord: {
      address: [{
        "given-name": "Bar",
        "organization": "Foo",
        "country": "US",
      }, {
        "given-name": "John",
        "family-name": "Doe",
        "organization": "Mozilla",
        "country": "US",
      }, {
        "given-name": "Foo",
        "organization": "Bar",
        "country": "US",
      }],
      creditCard: [{
        "cc-number": "4444000022220000",
        "cc-name": "John",
      }, {
        "cc-number": "4444000022221111",
        "cc-name": "Foo Bar",
        "cc-exp": "2026-26",
      }],
    },
  },

];

for (let testcase of TESTCASES) {
  add_task(async function() {
    do_print("Starting testcase: " + testcase.description);

    let doc = MockDocument.createTestDocument("http://localhost:8080/test/", testcase.document);
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
      Assert.deepEqual(record[type].map(secRecord => secRecord.record), expectedRecord[type]);
    }
  });
}
