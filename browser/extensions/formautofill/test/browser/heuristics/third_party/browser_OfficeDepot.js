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
            { fieldName: "family-name" },
            { fieldName: "organization" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2" },
            { fieldName: "postal-code" },
            { fieldName: "address-level2" }, // City & State
            { fieldName: "address-level2" }, // City
            { fieldName: "address-level1" }, // State
            { fieldName: "tel-area-code" },
            { fieldName: "tel-local-prefix" },
            { fieldName: "tel-local-suffix" },
            { fieldName: "tel-extension" },
            { fieldName: "email" },
          ],
        },
      ],
    },
    {
      fixturePath: "Payment.html",
      expectedResult: [
        {
          invalid: true, // because non of them is identified by fathom
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "cc-exp-month" },
            { fieldName: "cc-exp-year" },
            { fieldName: "cc-number" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "organization" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2" },
            { fieldName: "postal-code" },
            { fieldName: "address-level2" }, // City & State
            { fieldName: "address-level2" }, // City
            { fieldName: "address-level1" }, // state
            { fieldName: "tel-area-code" },
            { fieldName: "tel-local-prefix" },
            { fieldName: "tel-local-suffix" },
            { fieldName: "tel-extension" },
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
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/OfficeDepot/"
);
