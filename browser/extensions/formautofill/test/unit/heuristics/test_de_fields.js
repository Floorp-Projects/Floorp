/* global runHeuristicsTest */

"use strict";

runHeuristicsTest([
  {
    fixturePath: "heuristics_de_fields.html",
    expectedResult: [
      [
        [
          {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-name"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
        ],
      ],
    ],
  },
], "../../fixtures/");

