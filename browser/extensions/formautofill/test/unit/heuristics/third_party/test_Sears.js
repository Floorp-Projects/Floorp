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
      [ // credit card
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-name"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
      ],
      [ // Another billing address
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"}, // city
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"}, // state
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-extension"},
      ],
      [ // check out
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"}, // TODO: Wrong. This is for Driver's license.
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "bday-month"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "bday-day"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "bday-year"},
      ],
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ],
    ],
  },
], "../../../fixtures/third_party/Sears/");

