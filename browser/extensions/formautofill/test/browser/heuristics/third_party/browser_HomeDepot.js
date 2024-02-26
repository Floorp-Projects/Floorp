/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "Checkout_ShippingPayment.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "email" },
            { fieldName: "tel" },
            { fieldName: "street-address" },
            { fieldName: "postal-code" },
          ],
        },
        {
          invalid: true,
          fields: [
            {
              fieldName: "street-address",
              reason: "autocomplete",
              addressType: "billing",
            },
          ],
        },
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            { fieldName: "cc-exp-month" },
            { fieldName: "cc-exp-year" },
            { fieldName: "cc-number", reason: "fathom" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "cc-exp-month", reason: "regex-heuristic" }, // invisible
            { fieldName: "cc-exp-year", reason: "regex-heuristic" },  // invisible
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
            { fieldName: "email",reason: "regex-heuristic" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "email",reason: "regex-heuristic" },
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/HomeDepot/"
);
