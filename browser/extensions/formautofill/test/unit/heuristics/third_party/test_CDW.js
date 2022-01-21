/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
  [
    {
      fixturePath: "Checkout_ShippingInfo.html",
      expectedResult: [
        [
          [
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "given-name",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "family-name",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "organization",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "address-line1",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "address-line2",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "address-level2",
            }, // city
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "address-level1",
            }, // state
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "postal-code",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "email",
            },
            { section: "", addressType: "", contactType: "", fieldName: "tel" },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "tel-extension",
            },
          ],
        ],
        [],
      ],
    },
    {
      fixturePath: "Checkout_BillingPaymentInfo.html",
      expectedResult: [
        [
          [
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "given-name",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "family-name",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "organization",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "address-line1",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "address-line2",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "address-level2",
            }, // city
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "address-level1",
            }, // state
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "postal-code",
            },
          ],
          [
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-type",
            }, // ac-off
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-number",
            }, // ac-off
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-exp-month",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-exp-year",
            },
            //      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-csc"},
          ],
        ],
        [],
      ],
    },
    {
      fixturePath: "Checkout_Logon.html",
      expectedResult: [[], [], []],
    },
  ],
  "../../../fixtures/third_party/CDW/"
);
