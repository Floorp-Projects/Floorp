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
    description: `cc-number + first name with autocomplete attribute`,
    fixtureData: `
    <form>
      <input id="cc-number" autocomplete="cc-number">
      <input id="name" autocomplete="given-name">
    </form>`,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        fields: [
          {
            fieldName: "cc-number",
            reason: "autocomplete",
            autofill: TEST_PROFILE["cc-number"],
          },
          {
            fieldName: "cc-name",
            reason: "update-heuristic",
            autofill: TEST_PROFILE["cc-name"],
          },
        ],
      },
    ],
  },
  {
    description: `cc-exp + first name with autocomplete attribute`,
    fixtureData: `
    <form>
      <input id="cc-number" autocomplete="cc-number">
      <input id="cc-exp" autocomplete="cc-exp">
      <input id="name" autocomplete="given-name">
    </form>`,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        fields: [
          {
            fieldName: "cc-number",
            reason: "autocomplete",
            autofill: TEST_PROFILE["cc-number"],
          },
          {
            fieldName: "cc-exp",
            reason: "autocomplete",
            autofill: TEST_PROFILE["cc-exp"],
          },
          {
            fieldName: "cc-name",
            reason: "update-heuristic",
            autofill: TEST_PROFILE["cc-name"],
          },
        ],
      },
    ],
  },
  {
    description: `cc-number + first and last name with autocomplete attribute`,
    fixtureData: `
    <form>
      <input id="cc-number" autocomplete="cc-number">
      <input id="given" autocomplete="given-name">
      <input id="family" autocomplete="family-name">
    </form>`,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        fields: [
          {
            fieldName: "cc-number",
            reason: "autocomplete",
            autofill: TEST_PROFILE["cc-number"],
          },
          {
            fieldName: "cc-given-name",
            reason: "update-heuristic",
            autofill: "John",
          },
          {
            fieldName: "cc-family-name",
            reason: "update-heuristic",
            autofill: "Doe",
          },
        ],
      },
    ],
  },
  {
    description: `cc-number + first and last name without autocomplete attribute`,
    fixtureData: `
    <form>
      <input id="cc-number" autocomplete="cc-number">
      <input id="given" placeholder="First Name">
      <input id="family" placeholder="Last Name">
    </form>`,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        fields: [
          {
            fieldName: "cc-number",
            reason: "autocomplete",
            autofill: TEST_PROFILE["cc-number"],
          },
          {
            fieldName: "cc-given-name",
            reason: "update-heuristic",
            autofill: "John",
          },
          {
            fieldName: "cc-family-name",
            reason: "update-heuristic",
            autofill: "Doe",
          },
        ],
      },
    ],
  },
  {
    description: `previous field is a credit card name field`,
    fixtureData: `
    <form>
      <input id="cc-number" autocomplete="cc-number">
      <input id="cc-name" autocomplete="cc-name">
      <input id="given" autocomplete="given-name">
      <input id="family" autocomplete="family-name">
      <input id="address" autocomplete="street-address">
      <input id="country" autocomplete="country">
    </form>`,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        fields: [
          {
            fieldName: "cc-number",
            reason: "autocomplete",
            autofill: TEST_PROFILE["cc-number"],
          },
          {
            fieldName: "cc-name",
            reason: "autocomplete",
            autofill: TEST_PROFILE["cc-name"],
          },
        ],
      },
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "given-name" },
          { fieldName: "family-name" },
          { fieldName: "street-address" },
          { fieldName: "country" },
        ],
      },
    ],
  },
]);
