/* global runHeuristicsTest */

"use strict";

runHeuristicsTest(
  [
    {
      fixturePath: "Checkout_ShippingPayment.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "given-name" },
            { fieldName: "family-name" },
            { fieldName: "email" },
            { fieldName: "tel" },
            { fieldName: "street-address" },
            { fieldName: "postal-code" },
          ],
        },
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            // FIXME: bug 1392944 - the uncommented cc-exp-month and cc-exp-year are
            // both invisible <input> elements, and the following two <select>
            // elements are the correct ones. BTW, they are both applied
            // autocomplete attr.
            { fieldName: "cc-exp-month" },
            { fieldName: "cc-exp-year" },
            { fieldName: "cc-number", reason: "fathom" },
            // {fieldName: "cc-csc"},
          ],
        },
        {
          default: {
            reason: "autocomplete",
            addressType: "billing",
          },
          fields: [
            {
              fieldName: "street-address",
            }, // <select>
          ],
        },
      ],
    },
    {
      fixturePath: "SignIn.html",
      expectedResult: [
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "email" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "email" },
          ],
        },
      ],
    },
  ],
  "../../../fixtures/third_party/HomeDepot/"
);
