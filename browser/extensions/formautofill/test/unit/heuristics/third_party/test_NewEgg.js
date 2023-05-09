/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
  [
    {
      fixturePath: "ShippingInfo.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "country" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
            { fieldName: "tel" },
            { fieldName: "email" },
          ],
        },
      ],
    },
    {
      fixturePath: "BillingInfo.html",
      expectedResult: [
        {
          default: {
            reason: "fathom",
          },
          fields: [
            { fieldName: "cc-name" },
            { fieldName: "cc-number" }, // ac-off
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "country" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
            { fieldName: "tel" },
          ],
        },
        {
          default: {
            reason: "fathom",
          },
          fields: [
            { fieldName: "cc-name" },
            { fieldName: "cc-number" }, // ac-off
            { fieldName: "cc-exp-month", reason: "regex-heuristic" },
            { fieldName: "cc-exp-year", reason: "regex-heuristic" },
            // { fieldName: "cc-csc"},
          ],
        },
        {
          default: {
            reason: "fathom",
          },
          fields: [
            { fieldName: "cc-name" },
            { fieldName: "cc-number" }, // ac-off
            { fieldName: "cc-exp-month", reason: "regex-heuristic" },
            { fieldName: "cc-exp-year", reason: "regex-heuristic" },
          ],
        },
        {
          default: {
            reason: "fathom",
          },
          fields: [
            { fieldName: "cc-name" },
            { fieldName: "cc-number" }, // ac-off
          ],
        },
      ],
    },
    {
      fixturePath: "Login.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "email" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "email" },
          ],
        },
      ],
    },
  ],
  "../../../fixtures/third_party/NewEgg/"
);
