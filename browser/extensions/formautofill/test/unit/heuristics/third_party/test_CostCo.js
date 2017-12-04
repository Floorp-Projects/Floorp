/* global runHeuristicsTest */

"use strict";

runHeuristicsTest([
  {
    fixturePath: "ShippingAddress.html",
    expectedResult: [
      [],
      [],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "additional-name"}, // middle-name initial
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "organization"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"}, // city
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"}, // state
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "additional-name"}, // middle-name initial
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "organization"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"}, // city
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"}, // state
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
      [],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
      [],
    ],
  }, {
    fixturePath: "Payment.html",
    expectedResult: [
      [[
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-type"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"}, // ac-off
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"}, // ac-off
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-name"}, // ac-off
      ]],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"}, // ac-off
      ]],
      [],
      [],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "additional-name"}, // middle-name initial
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "organization"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"}, // city
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"}, // state
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "additional-name"}, // middle-name initial
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "organization"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"}, // state
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
      [],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
      [],
    ],
  }, {
    fixturePath: "SignIn.html",
    expectedResult: [
      [],
      [],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
      [],
      [[ // Forgot password
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "password"},
      ]],
      [[ // Sign up
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
      [],
    ],
  },
], "../../../fixtures/third_party/CostCo/");

