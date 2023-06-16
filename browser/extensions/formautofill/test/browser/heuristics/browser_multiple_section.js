/* global add_heuristic_tests */

"use strict";

add_heuristic_tests(
  [
    {
      fixturePath: "multiple_section.html",
      expectedResult: [
        {
          default: {
            reason: "autocomplete",
            addressType: "shipping",
          },
          fields: [
            { fieldName: "name", addressType: "" },
            { fieldName: "organization", addressType: "" },
            { fieldName: "street-address" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" },
            { fieldName: "postal-code" },
            { fieldName: "country" },
          ],
        },
        {
          default: {
            reason: "autocomplete",
            addressType: "billing",
          },
          fields: [
            { fieldName: "street-address" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" },
            { fieldName: "postal-code" },
            { fieldName: "country" },
          ],
        },
        {
          default: {
            reason: "autocomplete",
            section: "section-my",
          },
          fields: [
            { fieldName: "street-address" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" },
            { fieldName: "postal-code" },
            { fieldName: "country" },
            { fieldName: "tel", section: "", contactType: "work" },
            { fieldName: "email", section: "", contactType: "work" },
          ],
        },
        {
          invalid: true,
          default: {
            reason: "autocomplete",
          },
          fields: [
            // Even the `contactType` of these two fields are different with the
            // above two, we still consider they are identical until supporting
            // multiple phone number and email in one profile.
            { fieldName: "tel", contactType: "home" },
            { fieldName: "email", contactType: "home" },
          ],
        },
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            { fieldName: "name" },
            { fieldName: "organization" },
            { fieldName: "street-address" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" },
            { fieldName: "postal-code" },
            { fieldName: "country" },
          ],
        },
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            { fieldName: "street-address" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" },
            { fieldName: "postal-code" },
            { fieldName: "country" },
          ],
        },
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            { fieldName: "street-address" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" },
            { fieldName: "postal-code" },
            { fieldName: "country" },
            { fieldName: "tel", contactType: "work" },
            { fieldName: "email", contactType: "work" },
          ],
        },
        {
          invalid: true,
          default: {
            reason: "autocomplete",
            contactType: "home",
          },
          fields: [{ fieldName: "tel" }, { fieldName: "email" }],
        },
      ],
    },
  ],
  "fixtures/"
);
