/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test to ensure locale specific placeholders for credit card fields are properly used
 * to transform various values in the profile.
 */

"use strict";

const DEFAULT_CREDITCARD_RECORD = {
  guid: "123",
  "cc-exp-month": 1,
  "cc-exp-year": 2025,
  "cc-exp": "2025-01",
};

const getCCExpYearFormatted = () => {
  return DEFAULT_CREDITCARD_RECORD["cc-exp-year"].toString().substring(2);
};

const getCCExpMonthFormatted = () => {
  return DEFAULT_CREDITCARD_RECORD["cc-exp-month"].toString().padStart(2, "0");
};

const DEFAULT_EXPECTED_CREDITCARD_RECORD_SEPARATE_EXPIRY_FIELDS = {
  ...DEFAULT_CREDITCARD_RECORD,
  "cc-exp-month-formatted": getCCExpMonthFormatted(),
  "cc-exp-year-formatted": getCCExpYearFormatted(),
};

const FR_TESTCASES = [
  {
    description: "Use placeholder to adjust cc-exp format [mm/aa].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="mm/aa" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "01/25",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp format [mm / aa].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="mm / aa" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "01/25",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp format [MM / AA].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="MM / AA" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "01/25",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp format [mm / aaaa].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="mm / aaaa" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "01/2025",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp format [mm - aaaa].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="mm - aaaa" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "01-2025",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp format [aaaa-mm].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="aaaa-mm" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "2025-01",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp-year field [aa].",
    document: `<form>
                <input autocomplete="cc-number">
                <input autocomplete="cc-exp-month">
                <input autocomplete="cc-exp-year" placeholder="AA">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      { ...DEFAULT_EXPECTED_CREDITCARD_RECORD_SEPARATE_EXPIRY_FIELDS },
    ],
  },
];

const DE_TESTCASES = [
  {
    description: "Use placeholder to adjust cc-exp format [mm / jj].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="mm / jj" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "01/25",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp format [MM / JJ].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="MM / JJ" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "01/25",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp format [mm / jjjj].",
    document: `<form><input autocomplete="cc-number">
                 <input placeholder="mm / jjjj" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "01/2025",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp format [MM / JJJJ].",
    document: `<form><input autocomplete="cc-number">
                 <input placeholder="MM / JJJJ" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "01/2025",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp format [mm - jj].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="mm - jj" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "01-25",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp format [MM - JJ].",
    document: `<form><input autocomplete="cc-number">
               <input placeholder="MM - JJ" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "01-25",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp format [mm - jjjj].",
    document: `<form><input autocomplete="cc-number">
                 <input placeholder="mm - jjjj" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "01-2025",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp format [MM - JJJJ].",
    document: `<form><input autocomplete="cc-number">
                 <input placeholder="MM - JJJJ" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "01-2025",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp format [jjjj - mm].",
    document: `<form><input autocomplete="cc-number">
                 <input placeholder="jjjj - mm" autocomplete="cc-exp"></form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      Object.assign({}, DEFAULT_CREDITCARD_RECORD, {
        "cc-exp": "2025-01",
      }),
    ],
  },
  {
    description: "Use placeholder to adjust cc-exp-year field [jj].",
    document: `<form>
                <input autocomplete="cc-number">
                <input autocomplete="cc-exp-month">
                <input autocomplete="cc-exp-year" placeholder="JJ">
               </form>`,
    profileData: [Object.assign({}, DEFAULT_CREDITCARD_RECORD)],
    expectedResult: [
      { ...DEFAULT_EXPECTED_CREDITCARD_RECORD_SEPARATE_EXPIRY_FIELDS },
    ],
  },
];

const TESTCASES = [FR_TESTCASES, DE_TESTCASES];

for (let localeTests of TESTCASES) {
  for (let testcase of localeTests) {
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
      handler.focusedInput = form.elements[0];
      let adaptedRecords = handler.activeSection.getAdaptedProfiles(
        testcase.profileData
      );
      Assert.deepEqual(adaptedRecords, testcase.expectedResult);

      if (testcase.expectedOptionElements) {
        testcase.expectedOptionElements.forEach((expectedOptionElement, i) => {
          for (let field in expectedOptionElement) {
            let select = form.querySelector(`[autocomplete=${field}]`);
            let expectedOption = doc.getElementById(
              expectedOptionElement[field]
            );
            Assert.notEqual(expectedOption, null);

            let value = testcase.profileData[i][field];
            let cache =
              handler.activeSection._cacheValue.matchingSelectOption.get(
                select
              );
            let targetOption = cache[value] && cache[value].deref();
            Assert.notEqual(targetOption, null);

            Assert.equal(targetOption, expectedOption);
          }
        });
      }
    });
  }
}
