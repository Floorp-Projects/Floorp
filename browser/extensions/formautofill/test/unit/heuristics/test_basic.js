/* global runHeuristicsTest */

"use strict";

runHeuristicsTest([
  {
    fixturePath: "autocomplete_basic.html",
    expectedResult: [
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "organization"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ],
      [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "organization"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line3"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      ],
    ],
  },
], "../../fixtures/");

