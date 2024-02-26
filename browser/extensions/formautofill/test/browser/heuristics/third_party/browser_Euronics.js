/* global runHeuristicsTest */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "Payment.html",
      expectedResult: [
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            { fieldName: "cc-number" },
            //{ fieldName: "cc-cvc" },
            { fieldName: "cc-exp-month" },
            { fieldName: "cc-exp-year" },
            { fieldName: "cc-number", reason: "regex-heuristic" }, // invisible
            { fieldName: "cc-number", reason: "regex-heuristic" }, // invisible
          ],
        },
        {
          invalid: true,
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "cc-number" },
          ],
        },
        {
          invalid: true,
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "cc-number" },
            { fieldName: "cc-type" },
          ],
        },
        {
          invalid: true,
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "country" },
            { fieldName: "organization" },
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/Euronics/"
);
