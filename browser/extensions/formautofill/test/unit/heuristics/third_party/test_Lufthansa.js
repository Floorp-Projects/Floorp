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
              transform: "fullCCNumber => fullCCNumber.slice(0, 4)",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-number",
              transform: "fullCCNumber => fullCCNumber.slice(4, 8)",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-number",
              transform: "fullCCNumber => fullCCNumber.slice(8, 12)",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-number",
              transform: "fullCCNumber => fullCCNumber.slice(12, 16)",
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
