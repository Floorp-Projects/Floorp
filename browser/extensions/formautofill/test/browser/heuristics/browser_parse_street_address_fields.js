/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    description: "Apply heuristic when we only see one street-address fields",
    fixtureData: `
        <html><body>
          <form><input type="text" id="street-address"/></form>
          <form><input type="text" id="addr-1"/></form>
          <form><input type="text" id="addr-2"/></form>
          <form><input type="text" id="addr-3"/></form>
        </body></html>`,
    expectedResult: [
      {
        invalid: true,
        fields: [{ fieldName: "street-address", reason: "regex-heuristic" }],
      },
      {
        invalid: true,
        fields: [{ fieldName: "address-line1", reason: "regex-heuristic" }],
      },
      {
        invalid: true,
        fields: [{ fieldName: "address-line1", reason: "update-heuristic" }],
      },
      {
        invalid: true,
        fields: [{ fieldName: "address-line1", reason: "update-heuristic" }],
      },
    ],
  },
  {
    // Bug 1833613
    description:
      "street-address field is treated as address-line1 when address-line2 is present while adddress-line1 is not",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="street-address" autocomplete="street-address"/>
            <input type="text" id="address-line2" autocomplete="address-line2"/>
            <input type="text" id="email" autocomplete="email"/>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "address-line1", reason: "update-heuristic" },
          { fieldName: "address-line2", reason: "autocomplete" },
          { fieldName: "email", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    // Bug 1833613
    description:
      "street-address field should not be treated as address-line1 when address-line2 is not present",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="street-address" autocomplete="street-address"/>
            <input type="text" id="address-line3" autocomplete="address-line3"/>
            <input type="text" id="email" autocomplete="email"/>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "street-address", reason: "autocomplete" },
          { fieldName: "address-line3", reason: "autocomplete" },
          { fieldName: "email", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    // Bug 1833613
    description:
      "street-address field should not be treated as address-line1 when address-line1 is present",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="street-address" autocomplete="street-address"/>
            <input type="text" id="address-line1" autocomplete="address-line1"/>
            <input type="text" id="email" autocomplete="email"/>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        fields: [
          { fieldName: "street-address", reason: "autocomplete" },
          { fieldName: "address-line1", reason: "autocomplete" },
          { fieldName: "email", reason: "autocomplete" },
        ],
      },
    ],
  },
  {
    description:
      "street-address field is treated as address-line1 when address-line2 is present while adddress-line1 is not",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="addr-3"/>
            <input type="text" id="addr-2"/>
            <input type="text" id="addr-1"/>
          </form>
          <form>
            <input type="text" id="addr-3" autocomplete="address-line3"/>
            <input type="text" id="addr-2" autocomplete="address-line2"/>
            <input type="text" id="addr-1" autocomplete="address-line1"/>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        description:
          "Apply heuristic when we see 3 street-address fields occur in a row",
        fields: [
          { fieldName: "address-line1", reason: "update-heuristic" },
          { fieldName: "address-line2", reason: "regex-heuristic" },
          { fieldName: "address-line3", reason: "update-heuristic" },
        ],
      },
      {
        description:
          "Do not apply heuristic when we see 3 street-address fields occur in a row but autocomplete attribute is present",
        fields: [
          { fieldName: "address-line3", reason: "autocomplete" },
          { fieldName: "address-line2", reason: "autocomplete" },
          { fieldName: "address-line1", reason: "autocomplete" },
        ],
      },
    ],
  },
]);
