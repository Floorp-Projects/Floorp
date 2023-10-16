/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

add_heuristic_tests([
  {
    description: "All visual fields are considered focusable.",
    prefs: [
      [
        "extensions.formautofill.heuristics.interactivityCheckMode",
        "focusability",
      ],
    ],
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="name" autocomplete="name" />
            <input type="text" id="tel" autocomplete="tel" />
            <input type="text" id="email" autocomplete="email"/>
            <select id="country" autocomplete="country">
              <option value="United States">United States</option>
            </select>
            <input type="text" id="postal-code" autocomplete="postal-code"/>
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
    // ignore opacity (see  Bug 1835852),
    description:
      "Invisible fields with style.opacity=0 set are considered focusable.",
    prefs: [
      [
        "extensions.formautofill.heuristics.interactivityCheckMode",
        "focusability",
      ],
    ],
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="name" autocomplete="name" style="opacity:0" />
            <input type="text" id="tel" autocomplete="tel" />
            <input type="text" id="email" autocomplete="email" style="opacity:0"/>
            <select id="country" autocomplete="country">
              <option value="United States">United States</option>
            </select>
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
    description:
      "Some fields are considered unfocusable due to their invisibility.",
    prefs: [
      [
        "extensions.formautofill.heuristics.interactivityCheckMode",
        "focusability",
      ],
    ],
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="name" autocomplete="name" />
            <input type="text" id="tel" autocomplete="tel" />
            <input type="text" id="email" autocomplete="email" />
            <input type="text" id="country" autocomplete="country" />
            <input type="text" id="postal-code" autocomplete="postal-code" hidden />
            <input type="text" id="address-line1" autocomplete="address-line1" style="display:none" />
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
          { fieldName: "country" },
        ],
      },
    ],
  },
  {
    description: `Disabled field and field with tabindex="-1" is considered unfocusable`,
    prefs: [
      [
        "extensions.formautofill.heuristics.interactivityCheckMode",
        "focusability",
      ],
    ],
    fixtureData: `
        <html>
        <body>
          <form>
            <input type="text" id="name" autocomplete="name" />
            <input type="text" id="tel" autocomplete="tel" />
            <input type="text" id="email" autocomplete="email" />
            <input type="text" id="country" autocomplete="country" disabled/>
            <input type="text" id="postal-code" autocomplete="postal-code" tabindex="-1"/>
            <input type="text" id="address-line1" autocomplete="address-line1" />
            <input type="text" id="address-line2" autocomplete="address-line2" />
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
