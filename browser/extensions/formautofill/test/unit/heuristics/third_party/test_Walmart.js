/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
  [
    {
      fixturePath: "Checkout.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "postal-code" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "email" },
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
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "postal-code" },
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
            { fieldName: "address-line2" },
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
          ],
        },
      ],
    },
  ],
  "../../../fixtures/third_party/Walmart/"
);
