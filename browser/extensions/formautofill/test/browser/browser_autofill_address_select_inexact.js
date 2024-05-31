/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test ensures that an inexact substring match on an option label is performed
// when autofilling dropdown values.

const TEST_PROFILE = {
  "given-name": "Joe",
  "family-name": "Smith",
  "street-address": "7 First St",
  "address-level2": "Faketown",
  "address-level1": "Kansas",
  country: "US",
};

const MARKUP_SELECT_STATE = `
  <html><body>
    <form>
     <input id="given-name"><input id="family-name">
     <input id="street"><input id="city"><select id="state">
      <option value="ala1">State: Alabama
      <option value="ark2">State: Arkansas
      <option value="ida3">State: Idaho
      <option value="kns4">State: Kansas
      <option value="ney5">State: New York
      <option value="ore6">State: Oregon
      </select>
    </form>
  </body></html>
`;

add_autofill_heuristic_tests([
  {
    fixtureData: MARKUP_SELECT_STATE,
    profile: TEST_PROFILE,
    expectedResult: [
      {
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "given-name", autofill: TEST_PROFILE["given-name"] },
          { fieldName: "family-name", autofill: TEST_PROFILE["family-name"] },
          {
            fieldName: "street-address",
            autofill: TEST_PROFILE["street-address"],
          },
          {
            fieldName: "address-level2",
            autofill: TEST_PROFILE["address-level2"],
          },
          { fieldName: "address-level1", autofill: "kns4" },
        ],
      },
    ],
  },
]);
