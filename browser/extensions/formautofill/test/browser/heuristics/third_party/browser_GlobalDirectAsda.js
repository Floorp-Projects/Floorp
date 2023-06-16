/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "Payment.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "cc-number", reason: "fathom" },
            { fieldName: "cc-exp-month" },
            { fieldName: "cc-exp-year" },
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/GlobalDirectAsda/"
)
