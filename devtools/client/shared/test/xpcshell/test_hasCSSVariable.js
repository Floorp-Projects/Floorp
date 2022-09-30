/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";

// Test whether hasCSSVariable function of utils.js works correctly or not.

const {
  hasCSSVariable,
} = require("resource://devtools/client/inspector/rules/utils/utils.js");

function run_test() {
  info("Normal usage");
  ok(
    hasCSSVariable("var(--color)", "--color"),
    "Found --color variable in var(--color)"
  );
  ok(
    !hasCSSVariable("var(--color)", "--col"),
    "Did not find --col variable in var(--color)"
  );

  info("Variable with fallback");
  ok(
    hasCSSVariable("var(--color, red)", "--color"),
    "Found --color variable in var(--color)"
  );
  ok(
    !hasCSSVariable("var(--color, red)", "--col"),
    "Did not find --col variable in var(--color, red)"
  );

  info("Nested variables");
  ok(
    hasCSSVariable("var(--color1, var(--color2, blue))", "--color1"),
    "Found --color1 variable in var(--color1, var(--color2, blue))"
  );
  ok(
    hasCSSVariable("var(--color1, var(--color2, blue))", "--color2"),
    "Found --color2 variable in var(--color1, var(--color2, blue))"
  );
  ok(
    !hasCSSVariable("var(--color1, var(--color2, blue))", "--color"),
    "Did not find --color variable in var(--color1, var(--color2, blue))"
  );

  info("Invalid variable");
  ok(
    !hasCSSVariable("--color", "--color"),
    "Did not find --color variable in --color"
  );

  info("Variable with whitespace");
  ok(
    hasCSSVariable("var( --color )", "--color"),
    "Found --color variable in var( --color )"
  );
}
