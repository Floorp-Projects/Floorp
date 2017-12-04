/* global runHeuristicsTest */

"use strict";

runHeuristicsTest([
  {
    fixturePath: "multiple_section.html",
    expectedResult: [
      [
        [
          {"section": "", "addressType": "", "contactType": "", "fieldName": "name"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "organization"},
          {"section": "", "addressType": "", "contactType": "work", "fieldName": "tel"},
          {"section": "", "addressType": "", "contactType": "work", "fieldName": "email"},

          // Even the `contactType` of these two fields are different with the
          // above two, we still consider they are identical until supporting
          // multiple phone number and email in one profile.
//        {"section": "", "addressType": "", "contactType": "home", "fieldName": "tel"},
//        {"section": "", "addressType": "", "contactType": "home", "fieldName": "email"},
        ],
        [
          {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
          {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2"},
          {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level1"},
          {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "postal-code"},
          {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country"},
        ],
        [
          {"section": "", "addressType": "billing", "contactType": "", "fieldName": "street-address"},
          {"section": "", "addressType": "billing", "contactType": "", "fieldName": "address-level2"},
          {"section": "", "addressType": "billing", "contactType": "", "fieldName": "address-level1"},
          {"section": "", "addressType": "billing", "contactType": "", "fieldName": "postal-code"},
          {"section": "", "addressType": "billing", "contactType": "", "fieldName": "country"},
        ],
        [
          {"section": "section-my", "addressType": "", "contactType": "", "fieldName": "street-address"},
          {"section": "section-my", "addressType": "", "contactType": "", "fieldName": "address-level2"},
          {"section": "section-my", "addressType": "", "contactType": "", "fieldName": "address-level1"},
          {"section": "section-my", "addressType": "", "contactType": "", "fieldName": "postal-code"},
          {"section": "section-my", "addressType": "", "contactType": "", "fieldName": "country"},
        ],
      ],
      [
        [
          {"section": "", "addressType": "", "contactType": "", "fieldName": "name"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "organization"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
        ],
        [
          {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
        ],
        [
          {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level1"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "postal-code"},
          {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
          {"section": "", "addressType": "", "contactType": "work", "fieldName": "tel"},
          {"section": "", "addressType": "", "contactType": "work", "fieldName": "email"},
        ],
        [
          {"section": "", "addressType": "", "contactType": "home", "fieldName": "tel"},
          {"section": "", "addressType": "", "contactType": "home", "fieldName": "email"},
        ],
      ],
    ],
  },
], "../../fixtures/");

