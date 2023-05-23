/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "Checkout_ShippingAddress.html",
      expectedResult: [
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2" },
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
            { fieldName: "tel" },
          ],
        },
      ],
    },
    {
      fixturePath: "Checkout_Payment.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "cc-type" }, // ac-off
            { fieldName: "cc-number", reason: "fathom" }, // ac-off
            { fieldName: "cc-exp-month" }, // ac-off
            { fieldName: "cc-exp-year" }, // ac-off
            // {fieldName: "cc-csc"}, // ac-off
          ],
        },
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2" },
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
            { fieldName: "tel" },
            { fieldName: "email" },
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
            // Sign in
            { fieldName: "email", reason: "regex-heuristic"},
            // {fieldName: "password"},
          ],
        },
        {
          invalid: true,
          fields: [
            // Forgot password
            { fieldName: "email", reason: "regex-heuristic"},
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic"},
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/Macys/"
);
