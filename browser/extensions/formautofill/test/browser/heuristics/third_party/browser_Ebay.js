/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "Checkout_Payment_FR.html",
      expectedResult: [
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            { fieldName: "cc-number" },
            { fieldName: "cc-exp" },
            { fieldName: "cc-given-name" },
            { fieldName: "cc-family-name" },
          ],
        },
      ],
    },
  ],
  "fixtures/third_party/Ebay/"
)
