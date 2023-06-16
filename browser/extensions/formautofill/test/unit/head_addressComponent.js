"use strict";

/* exported BOTH_EMPTY, A_IS_EMPTY, B_IS_EMPTY, A_CONTAINS_B, B_CONTAINS_A, SIMILAR, SAME, DIFFERENT, runIsValidTest, runCompareTest */

const { AddressComparison, AddressComponent } = ChromeUtils.importESModule(
  "resource://gre/modules/shared/AddressComponent.sys.mjs"
);

const { FormAutofill } = ChromeUtils.importESModule(
  "resource://autofill/FormAutofill.sys.mjs"
);

const BOTH_EMPTY = AddressComparison.BOTH_EMPTY;
const A_IS_EMPTY = AddressComparison.A_IS_EMPTY;
const B_IS_EMPTY = AddressComparison.B_IS_EMPTY;
const A_CONTAINS_B = AddressComparison.A_CONTAINS_B;
const B_CONTAINS_A = AddressComparison.B_CONTAINS_A;
const SIMILAR = AddressComparison.SIMILAR;
const SAME = AddressComparison.SAME;
const DIFFERENT = AddressComparison.DIFFERENT;

function runIsValidTest(tests, fieldName, funcSetupRecord) {
  let region = FormAutofill.DEFAULT_REGION;
  for (const test of tests) {
    if (!Array.isArray(test)) {
      region = test.region;
      info(`Change region to ${JSON.stringify(test.region)}`);
      continue;
    }

    const [testValue, expected] = test;
    const record = funcSetupRecord(testValue);

    const field = new AddressComponent(record, region).getField(fieldName);
    const result = field.isValid();
    Assert.equal(
      result,
      expected,
      `Expect isValid returns ${expected} for ${testValue}`
    );
  }
}

function runCompareTest(tests, fieldName, funcSetupRecord) {
  let region = FormAutofill.DEFAULT_REGION;
  for (const test of tests) {
    if (!Array.isArray(test)) {
      info(`change region to ${JSON.stringify(test.region)}`);
      region = test.region;
      continue;
    }

    const [v1, v2, expected] = test;
    const r1 = funcSetupRecord(v1);
    const f1 = new AddressComponent(r1, region).getField(fieldName);

    const r2 = funcSetupRecord(v2);
    const f2 = new AddressComponent(r2, region).getField(fieldName);

    const result = AddressComparison.compare(f1, f2);
    const resultString = AddressComparison.resultToString(result);
    const expectedString = AddressComparison.resultToString(expected);
    Assert.equal(
      result,
      expected,
      `Expect ${expectedString} when comparing "${v1}" & "${v2}", got ${resultString}`
    );
  }
}
