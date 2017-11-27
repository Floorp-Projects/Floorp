/* global runHeuristicsTest */

"use strict";

runHeuristicsTest([
  {
    fixturePath: "ShippingAddress.html",
    expectedResult: [
      [],
      [], // search form, ac-off
      [ // ac-off
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ],
      [ // check-out, ac-off
/*
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-extension"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
*/
      ],
      [ // ac-off
/*
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-extension"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "new-password"},
*/
      ],
      [ // ac-off
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ],
      [ // ac-off
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ],
    ],
  }, {
    fixturePath: "PaymentOptions.html",
    expectedResult: [
      [],
      [], // search
      [[ // credit card
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
        // FIXME: bug 1392958 - Cardholder name field should be detected as cc-name
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-name"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
      ]],
      [[ // Another billing address
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"}, // city
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"}, // state
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-extension"},
      ]],
      [[ // check out
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},

        // FIXME: bug 1392950 - the bank routing number should not be detected
        // as cc-number.
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},

        // FIXME: bug 1392934 - this should be detected as address-level1 since
        // it's for Driver's license or state identification.
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"},

//      {"section": "", "addressType": "", "contactType": "", "fieldName": "bday-month"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "bday-day"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "bday-year"},
      ]],
      [[
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ]],
    ],
  },
], "../../../fixtures/third_party/Sears/");

