/* global runHeuristicsTest */

"use strict";

runHeuristicsTest([
  {
    fixturePath: "Checkout_ShippingPayment.html",
    expectedResult: [
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
        {"section": "", "addressType": "billing", "contactType": "", "fieldName": "street-address"}, // <select>
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"},
      ],
    ],
  }, {
    fixturePath: "SignIn.html",
    expectedResult: [
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ],
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ],
    ],
  },
], "../../../fixtures/third_party/HomeDepot/");

