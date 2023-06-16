/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_PROFILE_US = {
  email: "address_us@mozilla.org",
  organization: "Mozilla",
  country: "US",
};

const TEST_PROFILE_CA = {
  email: "address_ca@mozilla.org",
  organization: "Mozilla",
  country: "CA",
};

const MARKUP_SELECT_COUNTRY = `
  <html><body>
    <input id="email" autocomplete="email">
    <input id="organization" autocomplete="organization">
    <select id="country""" autocomplete="country">
      <option value="Germany">Germany</option>
      <option value="Canada">Canada</option>
      <option value="United States">United States</option>
      <option value="France">France</option>
    </select>
  </body></html>
`;

add_autofill_heuristic_tests([
  {
    fixtureData: MARKUP_SELECT_COUNTRY,
    profile: TEST_PROFILE_US,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE_US.email },
          { fieldName: "organization", autofill: TEST_PROFILE_US.organization },
          { fieldName: "country", autofill: "United States" },
        ],
      },
    ],
  },
  {
    fixtureData: MARKUP_SELECT_COUNTRY,
    profile: TEST_PROFILE_CA,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "email", autofill: TEST_PROFILE_CA.email },
          { fieldName: "organization", autofill: TEST_PROFILE_CA.organization },
          { fieldName: "country", autofill: "Canada" },
        ],
      },
    ],
  },
]);
