/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "YourInformation.html",
      expectedResult: [
        {
          invalid: true,
          fields: [
            { fieldName: "tel", reason: "regex-heuristic" },
            { fieldName: "email", reason: "regex-heuristic" },
            // { fieldName: "bday-month"}, // select
            // { fieldName: "bday-day"}, // select
            // { fieldName: "bday-year"},
          ],
        },
        {
          fields: [
            // { fieldName: "cc-csc"},
            { fieldName: "cc-type", reason: "regex-heuristic" },
            { fieldName: "cc-number", reason: "fathom" },
            { fieldName: "cc-exp", reason: "update-heuristic" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "cc-number", reason: "regex-heuristic" }, // txtQvcGiftCardNumber
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        },
      ],
    },
    {
      fixturePath: "PaymentMethod.html",
      expectedResult: [
        {
          invalid: true,
          fields: [
            { fieldName: "tel", reason: "regex-heuristic" },
            { fieldName: "email", reason: "regex-heuristic" },
            // { fieldName: "bday-month"}, // select
            // { fieldName: "bday-day"}, // select
            // { fieldName: "bday-year"}, // select
          ],
        },
        {
          default: {
            reason: "fathom",
          },
          fields: [
            { fieldName: "cc-type", reason: "regex-heuristic" }, // ac-off
            { fieldName: "cc-number" }, // ac-off
            { fieldName: "cc-exp", reason: "update-heuristic" },
            // { fieldName: "cc-csc"},
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "cc-number", reason: "regex-heuristic" }, // txtQvcGiftCardNumbe, ac-off
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        },
      ],
    },
    {
      fixturePath: "SignIn.html",
      expectedResult: [
        {
          // Sign in
          invalid: true,
          fields: [
            { fieldName: "email", reason: "regex-heuristic" },
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/QVC/"
);
