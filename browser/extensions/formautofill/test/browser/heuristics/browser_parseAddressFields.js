/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    // This bug happens only when the last element is address-lineX and
    // the field is identified by regular expressions in `HeuristicsRegExp` but is not
    // identified by regular expressions defined in `_parseAddressFields`
    fixtureData: `
        <html>
        <body>
          <form>
            <p><label>country: <input type="text" id="country" name="country" /></label></p>
            <p><label>tel: <input type="text" id="tel" name="tel" /></label></p>
            <p><label><input type="text" id="housenumber" /></label></p>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        description:
          "Address Line1 in the last element and is not updated in _parsedAddressFields",
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "country" },
          { fieldName: "tel" },
          { fieldName: "address-line1" },
        ],
      },
    ],
  },
  {
    fixtureData: `
        <html>
        <body>
          <form>
            <p><label>country: <input type="text" id="country" name="country" /></label></p>
            <p><label>tel: <input type="text" id="tel" name="tel" /></label></p>
            <p><label><input type="text" id="housenumber" /></label></p>
            <p><label><input type="text" id="addrcomplement" /></label></p>
          </form>
        </body>
        </html>`,
    expectedResult: [
      {
        description:
          "Address Line2 in the last element and is not updated in _parsedAddressFields",
        default: {
          reason: "regex-heuristic",
        },
        fields: [
          { fieldName: "country" },
          { fieldName: "tel" },
          { fieldName: "address-line1" },
          { fieldName: "address-line2" },
        ],
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
          { fieldName: "address-line1", reason: "regexp-heuristic" },
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
]);
