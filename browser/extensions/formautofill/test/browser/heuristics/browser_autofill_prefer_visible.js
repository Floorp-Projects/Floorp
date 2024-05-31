/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// This test ensures that when two dropdown fields exist and one is hidden, that the autofill
// is performed correctly on the visible one.

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
    First Name: <input id="fname">
    Last Name: <input id="lname">
    Address: <input id="address">
    <div id="unusedArea" style="display: none;">
      Country: <select id="unusedCountry" autocomplete="country">
          <option value="">Select a country</option>
          <option value="Australia">Australia</option>
          <option value="Indonesia">Indonesia</option>
          <option value="Japan">Japan</option>
          <option value="Malaysia">Malaysia</option>
          <option value="US">United States</option>
        </select>
    </div>
    <p>City: <input id="city"></p>
    <select id="country" autocomplete="country">
      <option value="">Select a country</option>
      <option value="Germany">Germany</option>
      <option value="Canada">Canada</option>
      <option value="United States">United States</option>
      <option value="France">France</option>
    </select>
  </body></html>
`;

/* global add_heuristic_tests */

add_heuristic_tests(
  [
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
            { fieldName: "country", autofill: "", reason: "autocomplete" },
            {
              fieldName: "address-level2",
              autofill: TEST_PROFILE["address-level2"],
            },
            {
              fieldName: "country",
              autofill: "United States",
              reason: "autocomplete",
            },
          ],
        },
      ],
    },
  ],
  "",
  { testAutofill: true }
);
