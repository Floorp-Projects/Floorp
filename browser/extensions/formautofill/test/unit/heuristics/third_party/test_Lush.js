/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
  [
    {
      fixturePath: "index.html",
      expectedResult: [
        [
          [
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-name",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-number",
            },
          ],
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
              fieldName: "cc-name",
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
  "../../../fixtures/third_party/Lush/"
);
