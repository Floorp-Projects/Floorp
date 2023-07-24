/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    description:
      "Apply credit card expiry heuristic only when there is only 1 credit card expiry field",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" autocomplete="cc-number"/>
            <input type="text" placeholder="mm-yy"/>
          </form>
          <form>
            <input type="text" autocomplete="cc-number"/>
            <input type="text" id="month"/>
          </form>
          <form>
            <input type="text" autocomplete="cc-number"/>
            <input type="text" id="year"/>
          </form>
        </body>
        </html>
      `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", reason: "autocomplete" },
          { fieldName: "cc-exp", reason: "regex-heuristic" },
        ],
      },
      {
        fields: [
          { fieldName: "cc-number", reason: "autocomplete" },
          { fieldName: "cc-exp", reason: "update-heuristic" },
        ],
      },
      {
        fields: [
          { fieldName: "cc-number", reason: "autocomplete" },
          { fieldName: "cc-exp", reason: "update-heuristic" },
        ],
      },
    ],
  },
  {
    description:
      "Do not apply credit card expiry heuristic only when the previous field is not a cc field",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" placeholder="mm-yy"/>
            <input type="text" autocomplete="cc-number"/>
          </form>
          <form>
            <input type="text" id="month"/>
            <input type="text" autocomplete="cc-number"/>
          </form>
          <form>
            <input type="text" id="year"/>
            <input type="text" autocomplete="cc-number"/>
          </form>
        </body>
        </html>
      `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-exp", reason: "regex-heuristic" },
          { fieldName: "cc-number", reason: "autocomplete" },
        ],
      },
      {
        fields: [{ fieldName: "cc-number", reason: "autocomplete" }],
      },
      {
        fields: [{ fieldName: "cc-number", reason: "autocomplete" }],
      },
    ],
  },
  {
    description:
      "Apply credit card expiry heuristic only when the previous fields has a credit card number field",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" autocomplete="cc-number"/>
            <input type="text" autocomplete="cc-name"/>
            <input type="text" placeholder="month"/>
          </form>
          <form>
            <input type="text" autocomplete="cc-number"/>
            <input type="text" autocomplete="cc-family-name"/>
            <input type="text" autocomplete="cc-given-name"/>
            <input type="text" placeholder="month"/>
          </form>
        </body>
        </html>
      `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", reason: "autocomplete" },
          { fieldName: "cc-name", reason: "autocomplete" },
          { fieldName: "cc-exp", reason: "update-heuristic" },
        ],
      },
      {
        fields: [
          { fieldName: "cc-number", reason: "autocomplete" },
          { fieldName: "cc-family-name", reason: "autocomplete" },
          { fieldName: "cc-given-name", reason: "autocomplete" },
          { fieldName: "cc-exp", reason: "update-heuristic" },
        ],
      },
    ],
  },
  {
    description:
      "Apply credit card expiry heuristic only when there are credit card expiry fields",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" autocomplete="cc-number"/>
            <input type="text" placeholder="month"/>
            <input type="text" placeholder="year"/>
          </form>
          <form>
            <input type="text" autocomplete="cc-number"/>
            <input type="text" placeholder="month"/>
            <input type="text" placeholder="month"/>
          </form>
          <form>
            <input type="text" autocomplete="cc-number"/>
            <input type="text" placeholder="year"/>
            <input type="text" placeholder="year"/>
          </form>
        </body>
        </html>
      `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", reason: "autocomplete" },
          { fieldName: "cc-exp-month", reason: "regex-heuristic" },
          { fieldName: "cc-exp-year", reason: "regex-heuristic" },
        ],
      },
      {
        fields: [
          { fieldName: "cc-number", reason: "autocomplete" },
          { fieldName: "cc-exp-month", reason: "regex-heuristic" },
          { fieldName: "cc-exp-year", reason: "update-heuristic" },
        ],
      },
      {
        fields: [
          { fieldName: "cc-number", reason: "autocomplete" },
          { fieldName: "cc-exp-month", reason: "update-heuristic" },
          { fieldName: "cc-exp-year", reason: "regex-heuristic" },
        ],
      },
    ],
  },
  {
    description: "Honor autocomplete attribute",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" autocomplete="cc-number"/>
            <input type="text" placeholder="month" autocomplete="cc-exp"/>
            <input type="text" placeholder="year" autocomplete="cc-exp"/>
          </form>
        </body>
        </html>
      `,
    expectedResult: [
      {
        fields: [
          { fieldName: "cc-number", reason: "autocomplete" },
          { fieldName: "cc-exp", reason: "autocomplete" },
        ],
      },
      {
        fields: [{ fieldName: "cc-exp", reason: "autocomplete" }],
      },
    ],
  },
]);
