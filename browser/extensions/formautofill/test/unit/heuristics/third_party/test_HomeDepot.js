/* global runHeuristicsTest */

"use strict";

runHeuristicsTest([
  {
    fixturePath: "Checkout_ShippingPayment.html",
    expectedResult: [
      [
        [
          {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
        ], [
          // FIXME: bug 1392944 - the uncommented cc-exp-month and cc-exp-year are
          // both invisible <input> elements, and the following two <select>
          // elements are the correct ones. BTW, they are both applied
          // autocomplete attr.
          {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
//        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
//        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},

//        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"},
        ], [
          {"section": "", "addressType": "billing", "contactType": "", "fieldName": "street-address"}, // <select>
        ],
      ],
    ],
  }, {
    fixturePath: "SignIn.html",
    expectedResult: [
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
    ],
  },
], "../../../fixtures/third_party/HomeDepot/");

