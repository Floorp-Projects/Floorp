/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

function checkObject(object, expected, message) {
  if (object && object.getGrip) {
    object = object.getGrip();
  }

  for (const name of Object.keys(expected)) {
    const expectedValue = expected[name];
    const value = object[name];
    checkValue(name, value, expectedValue, message);
  }
}

function checkValue(name, value, expected, message) {
  if (message) {
    message = ` for '${message}'`;
  }

  if (expected === null) {
    is(value, null, `'${name}' is null${message}`);
  } else if (expected === undefined) {
    is(value, expected, `'${name}' is undefined${message}`);
  } else if (
    typeof expected == "string" ||
    typeof expected == "number" ||
    typeof expected == "boolean"
  ) {
    is(value, expected, "property '" + name + "'" + message);
  } else if (expected instanceof RegExp) {
    ok(
      expected.test(value),
      name + ": " + expected + " matched " + value + message
    );
  } else if (Array.isArray(expected)) {
    info("checking array for property '" + name + "'" + message);
    checkObject(value, expected, message);
  } else if (typeof expected == "object") {
    info("checking object for property '" + name + "'" + message);
    checkObject(value, expected, message);
  }
}
