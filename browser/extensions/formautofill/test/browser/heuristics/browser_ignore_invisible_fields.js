/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    description: "all fields are visible",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="name" autocomplete="name" />
            <input type="text" id="tel" autocomplete="tel" />
            <input type="text" id="email" autocomplete="email" />
            <input type="text" id="country" autocomplete="country"/>
            <input type="text" id="postal-code" autocomplete="postal-code" />
            <input type="text" id="address-line1" autocomplete="address-line1" />
            <div>
              <input type="text" id="address-line2" autocomplete="address-line2" />
            </div>
          </form>
          </form>
        </body>
        </html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "name" },
          { fieldName: "tel" },
          { fieldName: "email" },
          { fieldName: "country" },
          { fieldName: "postal-code" },
          { fieldName: "address-line1" },
          { fieldName: "address-line2" },
        ],
      },
    ],
  },
  {
    description: "some fields are invisible because of css style",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="name" autocomplete="name" />
            <input type="text" id="tel" autocomplete="tel" />
            <input type="text" id="email" autocomplete="email" />
            <input type="text" id="country" autocomplete="country" hidden />
            <input type="text" id="postal-code" autocomplete="postal-code" style="display:none" />
            <input type="text" id="address-line1" autocomplete="address-line1" style="opacity:0" />
            <div style="visibility: hidden">
              <input type="text" id="address-line2" autocomplete="address-line2" />
            </div>
          </form>
        </body>
        </html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "name" },
          { fieldName: "tel" },
          { fieldName: "email" },
        ],
      },
    ],
  },
  {
    // hidden and style="display:none" are always considered regardless what visibility check we use
    description:
      "invisible fields are identified because number of elemenent in the form exceed the threshold",
    prefs: [["extensions.formautofill.heuristics.visibilityCheckThreshold", 1]],
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="name" autocomplete="name" />
            <input type="text" id="tel" autocomplete="tel" />
            <input type="text" id="email" autocomplete="email" />
            <input type="text" id="country" autocomplete="country" hidden />
            <input type="text" id="postal-code" autocomplete="postal-code" style="display:none" />
            <input type="text" id="address-line1" autocomplete="address-line1" style="opacity:0" />
            <div style="visibility: hidden">
              <input type="text" id="address-line2" autocomplete="address-line2" />
            </div>
          </form>
        </body>
        </html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "name" },
          { fieldName: "tel" },
          { fieldName: "email" },
          { fieldName: "address-line1" },
          { fieldName: "address-line2" },
        ],
      },
    ],
  },
]);
