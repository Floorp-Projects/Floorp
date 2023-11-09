/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    description:
      "Update name to cc-name when the previous section is a credit card section",
    fixtureData: `
        <html><body><form>
          <input id="cc-number" autocomplete="cc-number">
          <input id="given" placeholder="given-name">
          <input id="family" placeholder="family-name">
        </form></body></html>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", reason: "autocomplete" },
          { fieldName: "cc-given-name", reason: "update-heuristic" },
          { fieldName: "cc-family-name", reason: "update-heuristic" },
        ],
      },
    ],
  },
  {
    description:
      "Do not update name to cc-name when the previous credit card section already contains cc-name",
    fixtureData: `
        <html><body><form>
          <input id="cc-name" autocomplete="cc-name">
          <input id="cc-number" autocomplete="cc-number">
          <input id="given" placeholder="given-name">
          <input id="family" placeholder="family-name">
          <input id="address" autocomplete="street-address">
          <input id="country" autocomplete="country">
        </form></body></html>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-name", reason: "autocomplete" },
          { fieldName: "cc-number", reason: "autocomplete" },
        ],
      },
      {
        fields: [
          { fieldName: "given-name", reason: "regex-heuristic" },
          { fieldName: "family-name", reason: "regex-heuristic" },
          { fieldName: "street-address", reason: "autocomplete" },
          { fieldName: "country", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    description:
      "Do not update name to cc-name when the previous credit card section contains cc-csc",
    fixtureData: `
        <html><body><form>
          <input id="cc-number" autocomplete="cc-number">
          <input id="cc-csc" autocomplete="cc-csc">
          <input id="given" placeholder="given-name">
          <input id="family" placeholder="family-name">
          <input id="address" autocomplete="street-address">
          <input id="country" autocomplete="country">
        </form></body></html>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", reason: "autocomplete" },
          //{ fieldName: "cc-csc", reason: "autocomplete" },
        ],
      },
      {
        fields: [
          { fieldName: "given-name", reason: "regex-heuristic" },
          { fieldName: "family-name", reason: "regex-heuristic" },
          { fieldName: "street-address", reason: "autocomplete" },
          { fieldName: "country", reason: "autocomplete" },
        ],
      },
    ],
  },
]);
