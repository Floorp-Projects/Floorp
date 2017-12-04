/* global runHeuristicsTest */

"use strict";

runHeuristicsTest([
  {
    fixturePath: "Basic.html",
    expectedResult: [
      [ // ac-off
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-extension"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "organization"},
      ],
    ],
  }, {
    fixturePath: "Basic_ac_on.html",
    expectedResult: [
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-extension"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "organization"},
      ]],
    ],
  }, {
    fixturePath: "PaymentBilling.html",
    expectedResult: [
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"},
      ]],
    ],
  }, {
    fixturePath: "PaymentBilling_ac_on.html",
    expectedResult: [
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"},
      ]],
    ],
  },
], "../../../fixtures/third_party/Staples/");

