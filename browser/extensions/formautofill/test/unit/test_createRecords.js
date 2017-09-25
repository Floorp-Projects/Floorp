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
                <input id="organization" autocomplete="organization">
                <input id="country" autocomplete="country">
               </form>`,
    formValue: {
      "given-name": "John",
      "organization": "Mozilla",
      "country": "United States",
    },
    expectedRecord: {
      address: {
        "given-name": "John",
        "organization": "Mozilla",
        "country": "United States",
      },
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
      address: {
        "given-name": "John",
        "organization": "Mozilla",
        "country": "US",
      },
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
      address: {
        "given-name": "John",
        "organization": "Mozilla",
        "tel": "+11234567890",
      },
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
      address: {
        "given-name": "John",
        "organization": "Mozilla",
        "country": "United States",
        "tel": "",
      },
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
      address: {
        "given-name": "John",
        "organization": "Mozilla",
        "country": "United States",
        "tel": "",
      },
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
      address: {
        "given-name": "John",
        "organization": "Mozilla",
        "country": "United States",
        "tel": "",
      },
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
      address: undefined,
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
      address: undefined,
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
      creditCard: {
        "cc-number": "4444000022220000",
        "cc-name": "Foo Bar",
        "cc-exp": "2022-06",
      }
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
      creditCard: {
        "cc-number": "4444000022220000",
      },
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
      creditCard: undefined,
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
      creditCard: undefined,
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
