/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global runHeuristicsTest */

"use strict";

// This bug happens only when the last element is address-lineX and
// the field is identified by regular expressions in `HeuristicsRegExp` but is not
// identified by regular expressions defined in `_parseAddressFields`
const markup1 = `
  <html>
  <body>
    <form>
      <p><label>country: <input type="text" id="country" name="country" /></label></p>
      <p><label>tel: <input type="text" id="tel" name="tel" /></label></p>
      <p><label><input type="text" id="housenumber" /></label></p>
    </form>
  </body>
  </html>
`;

const markup2 = `
  <html>
  <body>
    <form>
      <p><label>country: <input type="text" id="country" name="country" /></label></p>
      <p><label>tel: <input type="text" id="tel" name="tel" /></label></p>
      <p><label><input type="text" id="housenumber" /></label></p>
      <p><label><input type="text" id="addrcomplement" /></label></p>
    </form>
  </body>
  </html>
`;

runHeuristicsTest(
  [
    {
      fixtureData: markup1,
      expectedResult: [
        {
          description: "Address Line1 in the last element and is not updated in _parsedAddressFields",
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
      fixtureData: markup2,
      expectedResult: [
        {
          description: "Address Line2 in the last element and is not updated in _parsedAddressFields",
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
  ]
);
