/*
 * Test for the normalization of records created by FormAutofillHandler.
 */

"use strict";

Cu.import("resource://formautofill/FormAutofillHandler.jsm");

const TESTCASES = [
  {
    description: "Don't create address record if filled data is less than 3",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="family-name" autocomplete="family-name">
                <input id="country" autocomplete="country">
               </form>`,
    formValue: {
      "given-name": "John",
      "family-name": "Doe",
    },
    expectedRecord: {
      address: undefined,
    },
  },
  {
    description: "\"country\" using @autocomplete shouldn't be identified aggressively",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="family-name" autocomplete="family-name">
                <input id="country" autocomplete="country">
               </form>`,
    formValue: {
      "given-name": "John",
      "family-name": "Doe",
      country: "United States",
    },
    expectedRecord: {
      address: {
        "given-name": "John",
        "family-name": "Doe",
        country: "United States",
      },
    },
  },
  {
    description: "\"country\" using heuristics should be identified aggressively",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="family-name" autocomplete="family-name">
                <input id="country" name="country">
               </form>`,
    formValue: {
      "given-name": "John",
      "family-name": "Doe",
      country: "United States",
    },
    expectedRecord: {
      address: {
        "given-name": "John",
        "family-name": "Doe",
        country: "US",
      },
    },
  },
  {
    description: "\"tel\" related fields should be concatenated",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="family-name" autocomplete="family-name">
                <input id="tel-country-code" autocomplete="tel-country-code">
                <input id="tel-national" autocomplete="tel-national">
               </form>`,
    formValue: {
      "given-name": "John",
      "family-name": "Doe",
      "tel-country-code": "+1",
      "tel-national": "1234567890",
    },
    expectedRecord: {
      address: {
        "given-name": "John",
        "family-name": "Doe",
        "tel": "+11234567890",
      },
    },
  },
  {
    description: "\"tel\" should be removed if it's too short",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="family-name" autocomplete="family-name">
                <input id="organization" autocomplete="organization">
                <input id="tel" autocomplete="tel-national">
               </form>`,
    formValue: {
      "given-name": "John",
      "family-name": "Doe",
      "organization": "Mozilla",
      "tel": "1234",
    },
    expectedRecord: {
      address: {
        "given-name": "John",
        "family-name": "Doe",
        "organization": "Mozilla",
        "tel": "",
      },
    },
  },
  {
    description: "\"tel\" should be removed if it's too long",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="family-name" autocomplete="family-name">
                <input id="organization" autocomplete="organization">
                <input id="tel" autocomplete="tel-national">
               </form>`,
    formValue: {
      "given-name": "John",
      "family-name": "Doe",
      "organization": "Mozilla",
      "tel": "1234567890123456",
    },
    expectedRecord: {
      address: {
        "given-name": "John",
        "family-name": "Doe",
        "organization": "Mozilla",
        "tel": "",
      },
    },
  },
  {
    description: "\"tel\" should be removed if it contains invalid characters",
    document: `<form>
                <input id="given-name" autocomplete="given-name">
                <input id="family-name" autocomplete="family-name">
                <input id="organization" autocomplete="organization">
                <input id="tel" autocomplete="tel-national">
               </form>`,
    formValue: {
      "given-name": "John",
      "family-name": "Doe",
      "organization": "Mozilla",
      "tel": "12345###!!!",
    },
    expectedRecord: {
      address: {
        "given-name": "John",
        "family-name": "Doe",
        "organization": "Mozilla",
        "tel": "",
      },
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

    for (let type in testcase.expectedRecord) {
      if (!testcase.expectedRecord[type]) {
        do_check_eq(record[type], undefined);
      } else {
        Assert.deepEqual(record[type].record, testcase.expectedRecord[type]);
      }
    }
  });
}
