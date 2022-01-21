/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
  [
    {
      fixturePath: "Checkout_Payment_FR.html",
      expectedResult: [
        [
          [
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-number",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-exp"
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-given-name"
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-family-name"
            },
          ],
        ],
      ],
    },
  ],
  "../../../fixtures/third_party/Ebay/"
)
