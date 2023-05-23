/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "heuristics_de_fields.html",
      expectedResult: [
        {
          fields: [
            { fieldName: "cc-name", reason: "fathom" },
            { fieldName: "cc-type", reason: "regex-heuristic" },
            { fieldName: "cc-number", reason: "fathom" },
            { fieldName: "cc-exp-month", reason: "regex-heuristic" },
            { fieldName: "cc-exp-year", reason: "regex-heuristic" },
          ],
        },
        {
          fields: [
            { fieldName: "cc-name", reason: "fathom" },
            { fieldName: "cc-type", reason: "regex-heuristic" },
            { fieldName: "cc-number", reason: "fathom" },
            { fieldName: "cc-exp-month", reason: "regex-heuristic" },
            { fieldName: "cc-exp-year", reason: "regex-heuristic" },
          ],
        },
      ],
    },
  ],
  "fixtures/"
);
