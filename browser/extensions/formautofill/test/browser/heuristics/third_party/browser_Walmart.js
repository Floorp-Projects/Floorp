/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "Checkout.html",
      expectedResult: [
        {
          invalid: true,
          fields: [
            { fieldName: "postal-code", reason: "regex-heuristic" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
            // { fieldName: "password"}, // ac-off
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "email" }, // ac-off
            // { fieldName: "password"},
            // { fieldName: "password"}, // ac-off
          ],
        },
      ],
    },
    {
      fixturePath: "Payment.html",
      expectedResult: [
        {
          default: {
            reason: "autocomplete",
            section: "section-payment",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "tel" },
          ],
        },
        {
          default: {
            reason: "autocomplete",
            section: "section-payment",
          },
          fields: [
            { fieldName: "cc-number" },
            { fieldName: "cc-exp-month" },
            { fieldName: "cc-exp-year" },
            // { fieldName: "cc-csc"},
          ],
        },
      ],
    },
    {
      fixturePath: "Shipping.html",
      expectedResult: [
        {
          invalid: true,
          fields: [
            { fieldName: "postal-code", reason: "regex-heuristic" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "tel" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2", reason:"update-heuristic" },
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/Walmart/"
);
