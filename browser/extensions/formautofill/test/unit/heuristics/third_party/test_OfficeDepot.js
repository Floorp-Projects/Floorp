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
            { fieldName: "family-name" },
            { fieldName: "organization" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2" },
            { fieldName: "postal-code" },
            { fieldName: "address-level2" },
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
      fixturePath: "Payment.html",
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
            { fieldName: "address-level2" },
            { fieldName: "address-level1" }, // state
            { fieldName: "tel-area-code" },
            { fieldName: "tel-local-prefix" },
            { fieldName: "tel-local-suffix" },
            { fieldName: "tel-extension" },
            { fieldName: "email" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "cc-exp-month" },
            { fieldName: "cc-exp-year" },
            // FIXME: bug 1392950 - the membership number should not be detected
            // as cc-number.
            { fieldName: "cc-number" },
          ],
        },
      ],
    },
    {
      fixturePath: "SignIn.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            // ac-off
            { fieldName: "email" }
          ]
        },
      ],
    },
  ],
  "../../../fixtures/third_party/OfficeDepot/"
);
