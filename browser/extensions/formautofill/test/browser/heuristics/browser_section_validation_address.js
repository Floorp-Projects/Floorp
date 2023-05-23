/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    description: `An address section is valid when it only contains more than three fields`,
    fixtureData: `
        <html><body>
            <input id="street-address" autocomplete="street-address">
            <input id="postal-code" autocomplete="postal-code">
            <input id="email" autocomplete="email">
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "email" },
        ],
      },
    ],
  },
  {
    description: `An address section is invalid when it contains less than threee fields`,
    fixtureData: `
        <html><body>
            <input id="postal-code" autocomplete="postal-code">
            <input id="email" autocomplete="email">

            <input id="postal-code" autocomplete="postal-code">
        </body></html>
      `,
    expectedResult: [
      {
        description: "A section with two fields",
        invalid: true,
        fields: [
          { fieldName: "postal-code", reason: "autocomplete" },
          { fieldName: "email", reason: "autocomplete" },
        ],
      },
      {
        description: "A section with one field",
        invalid: true,
        fields: [{ fieldName: "postal-code", reason: "autocomplete" }],
      },
    ],
  },
  {
    description: `Address section validation only counts the number of different address field name in the section`,
    fixtureData: `
        <html><body>
            <input id="postal-code" autocomplete="postal-code">
            <input id="email" autocomplete="email">
            <input id="email" autocomplete="email">
        </body></html>
      `,
    expectedResult: [
      {
        description:
          "A section with three fields but has duplicated email fields",
        invalid: true,
        fields: [
          { fieldName: "postal-code", reason: "autocomplete" },
          { fieldName: "email", reason: "autocomplete" },
          { fieldName: "email", reason: "autocomplete" },
        ],
      },
    ],
  },
]);
