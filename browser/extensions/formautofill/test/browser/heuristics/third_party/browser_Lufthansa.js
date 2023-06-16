/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "Checkout_Payment.html",
      expectedResult: [
        {
          default: {
            reason: "fathom",
          },
          fields: [
            { fieldName: "cc-type", reason: "regex-heuristic" },
            { fieldName: "cc-number" },
            { fieldName: "cc-number" },
            { fieldName: "cc-number" },
            { fieldName: "cc-number" },
            { fieldName: "cc-exp-month", reason: "regex-heuristic" },
            { fieldName: "cc-exp-year", reason: "regex-heuristic" },
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/Lufthansa/"
);
