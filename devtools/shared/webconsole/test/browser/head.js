/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from ../../../../client/shared/test/shared-head.js */

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/shared/test/shared-head.js",
  this
);

async function getTargetForTab(tab) {
  const target = await TargetFactory.forTab(tab);
  info("Attaching to the active tab.");
  await target.attach();
  return target;
}

function checkObject(object, expected) {
  for (const name of Object.keys(expected)) {
    const expectedValue = expected[name];
    const value = object[name];
    checkValue(name, value, expectedValue);
  }
}

function checkValue(name, value, expected) {
  if (expected === null) {
    is(value, null, "'" + name + "' is null");
  } else if (value === null) {
    ok(false, "'" + name + "' is null");
  } else if (value === undefined) {
    ok(false, "'" + name + "' is undefined");
  } else if (
    typeof expected == "string" ||
    typeof expected == "number" ||
    typeof expected == "boolean"
  ) {
    is(value, expected, "property '" + name + "'");
  } else if (expected instanceof RegExp) {
    ok(expected.test(value), name + ": " + expected + " matched " + value);
  } else if (Array.isArray(expected)) {
    info("checking array for property '" + name + "'");
    checkObject(value, expected);
  } else if (typeof expected == "object") {
    info("checking object for property '" + name + "'");
    checkObject(value, expected);
  }
}

function checkHeadersOrCookies(array, expected) {
  const foundHeaders = {};

  for (const elem of array) {
    if (!(elem.name in expected)) {
      continue;
    }
    foundHeaders[elem.name] = true;
    info("checking value of header " + elem.name);
    checkValue(elem.name, elem.value, expected[elem.name]);
  }

  for (const header in expected) {
    if (!(header in foundHeaders)) {
      ok(false, header + " was not found");
    }
  }
}
