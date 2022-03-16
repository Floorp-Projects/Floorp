/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
  [
    {
      fixturePath: "Payment.html",
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
              fieldName: "cc-name"
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-exp-month"
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-exp-year"
            },
          ],
        ],
      ],
    },
  ],
  "../../../fixtures/third_party/DirectAsda/"
)
