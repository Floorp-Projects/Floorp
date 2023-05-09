/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
  [
    {
      fixturePath: "YourInformation.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "tel"}, // ac-off
            { fieldName: "email" },
            // { fieldName: "bday-month"}, // select
            // { fieldName: "bday-day"}, // select
            // { fieldName: "bday-year"},
          ],
        },
        {
          default: {
            reason: "fathom",
          },
          fields: [
            { fieldName: "cc-type", reason: "regex-heuristic" },
            { fieldName: "cc-number" },
            { fieldName: "cc-exp", reason: "regex-heuristic" },
            // { fieldName: "cc-csc"},
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "cc-number" }, // txtQvcGiftCardNumber
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
    {
      fixturePath: "PaymentMethod.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "tel" }, // ac-off
            { fieldName: "email" },
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
            { fieldName: "cc-exp", reason: "regex-heuristic" },
            // { fieldName: "cc-csc"},
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "cc-number" }, // txtQvcGiftCardNumbe, ac-off
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
    {
      fixturePath: "SignIn.html",
      expectedResult: [
        {
          // Sign in
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
  "../../../fixtures/third_party/QVC/"
);
