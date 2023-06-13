/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "Checkout_ShippingInfo.html",
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
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
            { fieldName: "postal-code" }, // EXt
            { fieldName: "email" },
            { fieldName: "tel" },
            { fieldName: "tel-extension", reason: "update-heuristic" },
          ],
        },
      ],
    },
    {
      fixturePath: "Checkout_BillingPaymentInfo.html",
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
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
            { fieldName: "postal-code" }, // Ext
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "cc-type" }, // ac-off
            { fieldName: "cc-number", reason: "fathom" }, // ac-off
            { fieldName: "cc-exp-month" },
            { fieldName: "cc-exp-year" },
            // {fieldName: "cc-csc"},
          ],
        },
      ],
    },
    {
      fixturePath: "Checkout_Logon.html",
      expectedResult: [
      ],
    },
  ],
  "fixtures/third_party/CDW/"
);
