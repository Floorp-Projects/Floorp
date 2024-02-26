/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    description: `Create a new section when the section already has a field with the same field name`,
    fixtureData: `
        <html><body>
            <input type="text" autocomplete="cc-number"/>
            <input type="text" autocomplete="cc-name"/>
            <input type="text" autocomplete="cc-exp"/>
            <input type="text" autocomplete="cc-exp"/>
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
        ],
      },
      {
        fields: [{ fieldName: "cc-exp", reason: "autocomplete" }],
      },
    ],
  },
  {
    description: `Do not create a new section for an invisible field`,
    fixtureData: `
        <html><body>
            <input type="text" autocomplete="cc-number"/>
            <input type="text" autocomplete="cc-name"/>
            <input type="text" autocomplete="cc-exp"/>
            <input type="text" autocomplete="cc-exp" style="display:none"/>
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description: `Do not create a new section when the field with the same field name is an invisible field`,
    fixtureData: `
        <html><body>
            <input type="text" autocomplete="cc-number""/>
            <input type="text" autocomplete="cc-name"/>
            <input type="text" autocomplete="cc-exp" style="display:none"/>
            <input type="text" autocomplete="cc-exp"/>
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
          { fieldName: "cc-exp" },
        ],
      },
    ],
  },
  {
    description: `Do not create a new section for an invisible field (match field is not adjacent)`,
    fixtureData: `
        <html><body>
            <input type="text" autocomplete="cc-number"/>
            <input type="text" autocomplete="cc-name"/>
            <input type="text" autocomplete="cc-exp"/>
            <input type="text" autocomplete="cc-number" style="display:none"/>
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
          { fieldName: "cc-number" },
        ],
      },
    ],
  },
  {
    description: `Do not create a new section when the field with the same field name is an invisible field (match field is not adjacent)`,
    fixtureData: `
        <html><body>
            <input type="text" autocomplete="cc-number" style="display:none"/>
            <input type="text" autocomplete="cc-name"/>
            <input type="text" autocomplete="cc-exp"/>
            <input type="text" autocomplete="cc-number"/>
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "cc-number" },
          { fieldName: "cc-name" },
          { fieldName: "cc-exp" },
          { fieldName: "cc-number" },
        ],
      },
    ],
  },
]);
