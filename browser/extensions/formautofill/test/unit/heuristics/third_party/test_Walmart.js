/* global runHeuristicsTest */

"use strict";

runHeuristicsTest([
  {
    fixturePath: "Checkout.html",
    expectedResult: [
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
      ],
      [],
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "password"}, // ac-off
      ],
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "email"}, // ac-off
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "password"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "password"}, // ac-off
      ],
    ],
  }, {
    fixturePath: "Payment.html",
    expectedResult: [
      [
      ],
      [
        {"section": "section-payment", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "section-payment", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "section-payment", "addressType": "", "contactType": "", "fieldName": "cc-number"},
        {"section": "section-payment", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
        {"section": "section-payment", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"},
        {"section": "section-payment", "addressType": "", "contactType": "", "fieldName": "tel"},
      ],
    ],
  }, {
    fixturePath: "Shipping.html",
    expectedResult: [
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
      ],
      [
      ],
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"}, // city
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"}, // state
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
      ],
    ],
  },
], "../../../fixtures/third_party/Walmart/");

