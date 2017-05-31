/* global runHeuristicsTest */

"use strict";

runHeuristicsTest([
  {
    fixturePath: "Checkout_ShippingAddress.html",
    expectedResult: [
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"}, // city
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"}, // state
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
      ],
      [
/*
*/
      ],
    ],
  }, {
    fixturePath: "Checkout_Payment.html",
    expectedResult: [
      [
 /*
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-type"}, // ac-off
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"}, // ac-off
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"}, // ac-off
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"}, // ac-off
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"}, // ac-off
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"}, // ac-off
*/
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"}, // city
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"}, // state
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ],
      [],
    ],
  }, {
    fixturePath: "SignIn.html",
    expectedResult: [
      [ // Sign in
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "password"},
      ],
      [ // Forgot password
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ],
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ],
      [],
      [],
      [],
    ],
  },
], "../../../fixtures/third_party/Macys/");

