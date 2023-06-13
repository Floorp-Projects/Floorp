/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global add_heuristic_tests */

// Ensures that fields are identified correctly even when the containing form
// has its autocomplete attribute set to off.
add_heuristic_tests(
  [
    {
      fixturePath: "autocomplete_off_on_form.html",
      expectedResult: [
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            { fieldName: "organization" },
            { fieldName: "street-address" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" },
            { fieldName: "postal-code" },
            { fieldName: "country" },
            { fieldName: "tel" },
            { fieldName: "email" },
          ],
        },
        {
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "organization" },
            { fieldName: "address-line1" },
            { fieldName: "address-line2", reason: "update-heuristic" },
            { fieldName: "address-line3", reason: "update-heuristic" },
            { fieldName: "address-level2" },
            { fieldName: "address-level1" },
            { fieldName: "postal-code" },
            { fieldName: "country" },
            { fieldName: "tel" },
            { fieldName: "email" },
          ],
        },
        {
          default: {
            reason: "autocomplete",
          },
          fields: [
            { fieldName: "cc-number" },
            { fieldName: "cc-name" },
            { fieldName: "cc-exp-month" },
            { fieldName: "cc-exp-year" },
          ],
        },
        {
          invalid: true,
          default: {
            reason: "regex-heuristic",
          },
          fields: [
            { fieldName: "address-line1" },
            { fieldName: "address-level2" },
          ],
        },
        {
          invalid: true,
          fields: [
            { fieldName: "address-line1", reason: "update-heuristic" },
            { fieldName: "organization", reason: "regex-heuristic" },
          ],
        },
        {
          invalid: true,
          fields: [{ fieldName: "address-line1", reason: "update-heuristic" }],
        },
      ],
    },
  ],
  "fixtures/"
);
