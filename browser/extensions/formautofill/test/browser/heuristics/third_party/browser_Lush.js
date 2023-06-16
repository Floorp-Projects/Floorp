/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "index.html",
      expectedResult: [
        {
          default: {
            reason: "fathom",
          },
          fields: [
            { fieldName: "cc-name" },
            { fieldName: "cc-number" },
          ],
        },
        {
          fields: [
            { fieldName: "cc-number", reason: "autocomplete" },
            { fieldName: "cc-name", reason: "fathom" },
            { fieldName: "cc-exp-month", reason: "regex-heuristic" },
            { fieldName: "cc-exp-year", reason: "regex-heuristic" },
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/Lush/"
);
