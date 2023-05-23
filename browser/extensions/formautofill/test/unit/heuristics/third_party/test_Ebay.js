/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
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
  "../../../fixtures/third_party/Ebay/"
)
