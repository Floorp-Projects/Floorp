/* global runHeuristicsTest */

"use strict";

runHeuristicsTest([
  {
    fixturePath: "Checkout_ShippingAddress.html",
    expectedResult: [
      [], // Search form
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"}, // city
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"}, // state
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
      ]],
      [[ // Sign up
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
      [[ // unknown
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
      ]],
    ],
  }, {
    fixturePath: "Checkout_Payment.html",
    expectedResult: [
      [], // Search form
      [[ // Sign up
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"}, // city
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"}, // state
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
      ]],
      [[
        // unknown
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
      ]],
    ],
  }, {
    fixturePath: "SignIn.html",
    expectedResult: [
      [[ // Sign in
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
    ],
  },
], "../../../fixtures/third_party/BestBuy/");

