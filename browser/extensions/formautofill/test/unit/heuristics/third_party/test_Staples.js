/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
  [
    {
      fixturePath: "Basic.html",
      expectedResult: [
        {
          // ac-off
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "address-line1" },
            { fieldName: "email" },
            { fieldName: "tel" },
            // {fieldName: "tel-extension"},
            { fieldName: "organization" },
          ]
        },
      ],
    },
    {
      fixturePath: "Basic_ac_on.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "address-line1" },
            { fieldName: "email" },
            { fieldName: "tel" },
            // {fieldName: "tel-extension"},
            { fieldName: "organization" },
          ],
        },
      ],
    },
    {
      fixturePath: "PaymentBilling.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "cc-number", reason: "fathom" },
            { fieldName: "cc-exp" },
            // {fieldName: "cc-csc"},
          ],
        },
      ],
    },
    {
      fixturePath: "PaymentBilling_ac_on.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "cc-number", reason: "fathom" },
            { fieldName: "cc-exp" },
            // { fieldName: "cc-csc"},
          ],
        },
      ],
    },
  ],
  "../../../fixtures/third_party/Staples/"
);
