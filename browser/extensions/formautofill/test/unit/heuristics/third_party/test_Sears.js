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
            { fieldName: "email" },
          ]
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            // check-out, ac-off
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" },
            { fieldName: "postal-code" },
            { fieldName: "tel" },
            { fieldName: "tel-extension" },
            { fieldName: "email" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            // ac-off
            // { fieldName: "email"},
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" },
            { fieldName: "postal-code" },
            { fieldName: "tel" },
            { fieldName: "tel-extension" },
            // { fieldName: "new-password"},
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
          // ac-off
            {  fieldName: "email" },
          ]
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            // ac-off
            { fieldName: "email" },
          ]
        },
      ],
    },
    {
      fixturePath: "PaymentOptions.html",
      expectedResult: [
        {
          default: {
            reason: "fathom",
          },
          fields: [
            { fieldName: "cc-number" },
            { fieldName: "cc-name" },
            // {fieldName: "cc-csc"},
            { fieldName: "cc-exp-month", reason: "regex-heuristic" },
            { fieldName: "cc-exp-year", reason: "regex-heuristic" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            // Another billing address
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2" },
            { fieldName: "address-level2" }, // city
            { fieldName: "address-level1" }, // state
            { fieldName: "postal-code" },
            { fieldName: "tel" },
            { fieldName: "tel-extension" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            // check out
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            // FIXME: bug 1392934 - this should be detected as address-level1 since
            // it's for Driver's license or state identification.
            { fieldName: "address-level1" },
            // { fieldName: "bday-month"},
            // { fieldName: "bday-day"},
            // { fieldName: "bday-year"},
          ],
        },
        {
          default: {
            reason: "fathom",
          },
          fields: [
            // FIXME: bug 1392950 - the bank routing number should not be detected
            // as cc-number.
            { fieldName: "cc-number" },
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
  ],
  "../../../fixtures/third_party/Sears/"
);
