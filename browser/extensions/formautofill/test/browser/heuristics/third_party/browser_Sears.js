/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "ShippingAddress.html",
      expectedResult: [
        {
          invalid: true,
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
            { fieldName: "tel-extension", reason: "update-heuristic" },
            { fieldName: "email" },
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
            { fieldName: "tel-extension", reason: "update-heuristic" },
            // { fieldName: "new-password"},
          ],
        },
        {
          invalid: true,
          default: {
            reason: "regex-heuristic",
          },
          fields: [
          // ac-off
            {  fieldName: "email" },
          ]
        },
        {
          invalid: true,
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
  ],
  "fixtures/third_party/Sears/"
);
