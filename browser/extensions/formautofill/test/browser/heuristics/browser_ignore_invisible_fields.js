/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    description: "Visible",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="name" autocomplete="name" />
            <input type="text" id="tel" autocomplete="tel" />
            <input type="text" id="email" autocomplete="email" />
            <input type="text" id="street-address" autocomplete="street-address" />
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
          { fieldName: "street-address" },
          { fieldName: "address-line1" },
          { fieldName: "address-line2" },
        ],
      },
    ],
  },
  {
    description: "Invisible",
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="name" autocomplete="name" />
            <input type="text" id="tel" autocomplete="tel" />
            <input type="text" id="email" autocomplete="email" />
            <input type="text" id="street-address" autocomplete="street-address" style="opacity:0" />
            <input type="text" id="address-line1" autocomplete="address-line1" style="display:none" />
            <div style="content-visibility:hidden">
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
    description: "Invisible",
    prefs: [["extensions.formautofill.heuristics.visibleCheckThreshold", 1]],
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="name" autocomplete="name" />
            <input type="text" id="tel" autocomplete="tel" />
            <input type="text" id="email" autocomplete="email" />
            <input type="text" id="street-address" autocomplete="street-address" style="opacity:0" />
            <input type="text" id="address-line1" autocomplete="address-line1" style="display:none" />
            <div style="content-visibility:hidden">
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
          { fieldName: "street-address" },
          { fieldName: "address-line1" },
          { fieldName: "address-line2" },
        ],
      },
    ],
  },
]);
