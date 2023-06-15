/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
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
            { fieldName: "address-line2", reason:"update-heuristic" },
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
            { fieldName: "tel" },
            { fieldName: "email" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "address-line1", reason:"regex-heuristic" },
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
            { fieldName: "address-line2", reason:"update-heuristic" },
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
            { fieldName: "tel" },
            { fieldName: "email" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "address-line1", reason:"regex-heuristic" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
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
          invalid: true,  // confidence is not high enough
          fields: [
            { fieldName: "cc-number", reason: "fathom" }, // ac-off
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
            { fieldName: "address-line2", reason:"update-heuristic" },
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
            { fieldName: "tel" },
            { fieldName: "email" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "address-line1", reason:"regex-heuristic" },
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
            { fieldName: "address-line2", reason:"update-heuristic" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
            { fieldName: "tel" },
            { fieldName: "email" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "address-line1", reason:"regex-heuristic" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        },
      ],
    },
    {
      fixturePath: "SignIn.html",
      expectedResult: [
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        },
        {
          invalid: true,
          fields: [
            // Forgot password
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
            // {fieldName: "password"},
          ],
        },
        {
          invalid: true,
          fields: [
            // Sign up
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        }
      ],
    },
  ],
  "fixtures/third_party/CostCo/"
);
