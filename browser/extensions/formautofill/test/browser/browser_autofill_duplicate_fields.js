/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PROFILE = {
  "given-name": "Timothy",
  "additional-name": "John",
  "family-name": "Berners-Lee",
  organization: "Mozilla",
  "street-address": "331 E Evelyn Ave",
  "address-level2": "Mountain View",
  "address-level1": "CA",
  "postal-code": "94041",
  country: "US",
  tel: "+16509030800",
  email: "address@mozilla.org",
};

add_autofill_heuristic_tests([
  {
    fixtureData: `
        <html><body>
          <input id="email" autocomplete="email">
          <input id="postal-code" autocomplete="postal-code">
          <input id="country" autocomplete="country">
        </body></html>
      `,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "postal-code", autofill: TEST_PROFILE["postal-code"] },
          { fieldName: "country", autofill: TEST_PROFILE.country },
        ],
      },
    ],
  },
  {
    description: "autofill multiple email fields(2)",
    fixtureData: `
        <html><body>
          <input id="email" autocomplete="email">
          <input id="email" autocomplete="email">
          <input id="postal-code" autocomplete="postal-code">
          <input id="country" autocomplete="country">
        </body></html>
      `,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "postal-code", autofill: TEST_PROFILE["postal-code"] },
          { fieldName: "country", autofill: TEST_PROFILE.country },
        ],
      },
    ],
  },
  {
    description: "autofill multiple email fields(3)",
    fixtureData: `
        <html><body>
          <input id="email" autocomplete="email">
          <input id="email" autocomplete="email">
          <input id="email" autocomplete="email">
          <input id="postal-code" autocomplete="postal-code">
          <input id="country" autocomplete="country">
        </body></html>
      `,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "email", autofill: TEST_PROFILE.email },
          { fieldName: "postal-code", autofill: TEST_PROFILE["postal-code"] },
          { fieldName: "country", autofill: TEST_PROFILE.country },
        ],
      },
    ],
  },
]);
