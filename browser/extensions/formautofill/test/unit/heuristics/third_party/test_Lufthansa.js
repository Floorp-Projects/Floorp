/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
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
  "../../../fixtures/third_party/Lufthansa/"
);
