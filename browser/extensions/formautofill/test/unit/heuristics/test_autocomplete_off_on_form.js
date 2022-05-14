/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global runHeuristicsTest */

// Ensures that fields are identified correctly even when the containing form
// has its autocomplete attribute set to off.
runHeuristicsTest(
  [
    {
      fixturePath: "autocomplete_off_on_form.html",
      expectedResult: [
        [
          [
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
              fieldName: "street-address",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "address-level2",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "address-level1",
            },
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
              fieldName: "country",
            },
            { section: "", addressType: "", contactType: "", fieldName: "tel" },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "email",
            },
          ],
        ],
        [
          [
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
              fieldName: "address-line3",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "address-level2",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "address-level1",
            },
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
              fieldName: "country",
            },
            { section: "", addressType: "", contactType: "", fieldName: "tel" },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "email",
            },
          ],
          [
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-number",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "cc-name",
            },
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
          ],
        ],
        [
          [
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
              fieldName: "address-level2",
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
              fieldName: "organization",
            },
            {
              section: "",
              addressType: "",
              contactType: "",
              fieldName: "address-line3",
            },
          ],
        ],
      ],
    },
  ],
  "../../fixtures/"
);
