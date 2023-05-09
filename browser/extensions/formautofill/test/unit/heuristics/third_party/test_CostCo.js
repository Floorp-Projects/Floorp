/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
  [
    {
      fixturePath: "ShippingAddress.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "additional-name" }, // middle-name initial
            { fieldName: "family-name" },
            { fieldName: "organization" },
            { fieldName: "country" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2" },
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
            { fieldName: "tel" },
            { fieldName: "email" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "additional-name" }, // middle-name initial
            { fieldName: "family-name" },
            { fieldName: "organization" },
            { fieldName: "country" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2" },
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
            { fieldName: "tel" },
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
    {
      fixturePath: "Payment.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "cc-type" }, // ac-off
            { fieldName: "cc-number", reason: "fathom" }, // ac-off
            { fieldName: "cc-exp-month" },
            { fieldName: "cc-exp-year" },
            // { fieldName: "cc-csc"}, // ac-off
            { fieldName: "cc-name", reason: "fathom" }, // ac-off
          ],
        },
        {
          default: {
            reason: "fathom",
          },
          fields: [
            { fieldName: "cc-number" }, // ac-off
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "additional-name" }, // middle-name initial
            { fieldName: "family-name" },
            { fieldName: "organization" },
            { fieldName: "country" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2" },
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
            { fieldName: "tel" },
            { fieldName: "email" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "additional-name" }, // middle-name initial
            { fieldName: "family-name" },
            { fieldName: "organization" },
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
    {
      fixturePath: "SignIn.html",
      expectedResult: [
        {
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        },
        {
          fields: [
            // Forgot password
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        },
        {
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        },
        {
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
            // {fieldName: "password"},
          ],
        },
        {
          fields: [
            // Sign up
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        },
        {
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        }
      ],
    },
  ],
  "../../../fixtures/third_party/CostCo/"
);
