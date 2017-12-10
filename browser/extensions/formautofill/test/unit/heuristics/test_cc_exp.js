/* global runHeuristicsTest */

"use strict";

runHeuristicsTest([
  {
    fixturePath: "heuristics_cc_exp.html",
    expectedResult: [
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
      ]],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp"},
      ]],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp"},
      ]],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
      ]],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
      ]],
    ],
  },
], "../../fixtures/");

