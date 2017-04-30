/* global runHeuristicsTest */

"use strict";

runHeuristicsTest([
  {
    fixturePath: "YourInformation.html",
    expectedResult: [
      [
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"}, // ac-off
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "bday-year"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-type"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
      ],
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ],
    ],
  }, {
    fixturePath: "PaymentMethod.html",
    expectedResult: [
      [
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"}, // ac-off
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "bday-month"}, // select
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "bday-day"}, // select
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "bday-year"}, // select
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-type"}, // select
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp"}, // select
//      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"},
      ],
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ],
    ],
  }, {
    fixturePath: "SignIn.html",
    expectedResult: [
      [
        // Unknown
      ],
      [ // Sign in
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ],
      [],
    ],
  },
], "../../../fixtures/third_party/QVC/");

