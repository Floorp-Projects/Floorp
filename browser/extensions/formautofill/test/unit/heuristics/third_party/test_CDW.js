/* global runHeuristicsTest */

"use strict";

runHeuristicsTest([
  {
    fixturePath: "Checkout_ShippingInfo.html",
    expectedResult: [
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "organization"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"}, // city
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"}, // state
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},

        // FIXME: bug 1392932 - misdetect ZIP ext string
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-extension"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
      ], [
        // The below "tel-extension" is correct and removed due to the
        // duplicated field above.
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-extension"},
      ]],
      [],
    ],
  }, {
    fixturePath: "Checkout_BillingPaymentInfo.html",
    expectedResult: [
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "organization"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"}, // city
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"}, // state
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},

        // FIXME: bug 1392932 - misdetect ZIP ext string
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-extension"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-type"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"}, // ac-off
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},

//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"},
      ], [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-extension"},
      ]],
      [],
    ],
  }, {
    fixturePath: "Checkout_Logon.html",
    expectedResult: [
      [],
      [],
      [],
    ],
  },
], "../../../fixtures/third_party/CDW/");

