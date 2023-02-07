/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that css-color-db matches platform.

"use strict";

const { cssColors } = require("resource://devtools/shared/css/color-db.js");

add_task(() => {
  for (const name in cssColors) {
    ok(
      InspectorUtils.isValidCSSColor(name),
      name + " is valid in InspectorUtils"
    );
  }
});
