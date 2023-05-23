/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "Checkout_ShippingAddress.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "street-address" },
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic" }, // sign-up
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
            { fieldName: "tel", reason: "regex-heuristic" },
          ],
        },
      ],
    },
    {
      fixturePath: "Checkout_Payment.html",
      expectedResult: [
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "street-address" },
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
            { fieldName: "tel", reason: "regex-heuristic" },
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
            { fieldName: "email", reason: "regex-heuristic" }, // sign-in
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/BestBuy/"
);
