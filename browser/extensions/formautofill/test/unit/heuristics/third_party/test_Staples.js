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
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-extension"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "organization"},
      ],
    ],
  }, {
    fixturePath: "PaymentBilling.html",
    expectedResult: [
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},

        // FIXME: bug 1392940 - Since any credit card fields should be
        // recognized no matter it's autocomplete="off" or not. This field
        // "cc-exp-month" should be fixed as "cc-exp".
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"},
      ],
    ],
  }, {
    fixturePath: "PaymentBilling_ac_on.html",
    expectedResult: [
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},

        // FIXME: bug 1392940 - Since any credit card fields should be
        // recognized no matter it's autocomplete="off" or not. This field
        // "cc-exp-month" should be fixed as "cc-exp".
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"},
      ],
    ],
  },
], "../../../fixtures/third_party/Staples/");

