/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PROFILE = {
  "cc-name": "John Doe",
  "cc-number": "4111111111111111",
  // "cc-type" should be remove from proile after fixing Bug 1834768.
  "cc-type": "visa",
  "cc-exp-month": 4,
  "cc-exp-year": new Date().getFullYear(),
};

add_setup(async function () {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.formautofill.creditCards.supported", "on"],
      ["extensions.formautofill.creditCards.enabled", true],
    ],
  });
});

add_autofill_heuristic_tests([
  {
    description:
      "cc-type select does not have any information in labels or attributes",
    fixtureData: `
    <form>
      <input id="cc-number" autocomplete="cc-number">
      <input id="cc-name" autocomplete="cc-name">
      <select id="test">
        <option value="" selected="">0</option>
        <option value="VISA">1</option>
        <option value="MasterCard">2</option>
        <option value="DINERS">3</option>
        <option value="Discover">4</option>
     </select>
    </form>
    <form>
      <input id="cc-number" autocomplete="cc-number">
      <input id="cc-name" autocomplete="cc-name">
      <select id="test">
        <option value="0" selected="">Card Type</option>
        <option value="1">Visa</option>
        <option value="2">MasterCard</option>
        <option value="3">Diners Club International</option>
        <option value="4">Discover</option>
     </select>
    </form>`,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        description: "cc-type option.value has the hint",
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
          { fieldName: "cc-type", reason: "regex-heuristic", autofill: "VISA" },
        ],
      },
      {
        description: "cc-type option.text has the hint",
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-number", autofill: TEST_PROFILE["cc-number"] },
          { fieldName: "cc-name", autofill: TEST_PROFILE["cc-name"] },
          { fieldName: "cc-type", reason: "regex-heuristic", autofill: "1" },
        ],
      },
    ],
  },
]);
