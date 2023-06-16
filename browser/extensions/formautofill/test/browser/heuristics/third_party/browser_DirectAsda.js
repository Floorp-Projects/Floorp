/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "Payment.html",
      expectedResult: [
        {
          default: {
            reason: "fathom",
          },
          fields: [
            { fieldName: "cc-number" },
            { fieldName: "cc-name" },
            { fieldName: "cc-exp-month", reason: "regex-heuristic" },
            { fieldName: "cc-exp-year", reason: "regex-heuristic" },
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/DirectAsda/"
)
