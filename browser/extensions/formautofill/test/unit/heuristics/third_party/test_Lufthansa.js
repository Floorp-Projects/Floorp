/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
  [
    {
      fixturePath: "Checkout_Payment.html",
      expectedResult: [
        [
          [
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-type",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-number",
              part: 1,
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-number",
              part: 2,
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-number",
              part: 3,
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-number",
              part: 4,
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-exp-month",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-exp-year",
            },
          ],
        ],
      ],
    },
  ],
  "../../../fixtures/third_party/Lufthansa/"
);
