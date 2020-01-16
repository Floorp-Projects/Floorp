/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper positioned elements test cases.

// These are the properties we care about, those that are inactive when the element isn't
// positioned.
const PROPERTIES = [
  { property: "z-index", value: "2" },
  { property: "top", value: "20px" },
  { property: "right", value: "20px" },
  { property: "bottom", value: "20px" },
  { property: "left", value: "20px" },
];

// These are all of the different position values and whether the above properties are
// active or not for each.
const POSITIONS = [
  { position: "unset", isActive: false },
  { position: "initial", isActive: false },
  { position: "inherit", isActive: false },
  { position: "static", isActive: false },
  { position: "absolute", isActive: true },
  { position: "relative", isActive: true },
  { position: "fixed", isActive: true },
  { position: "sticky", isActive: true },
];

function makeTestCase(property, value, position, isActive) {
  return {
    info: `${property} is ${
      isActive ? "" : "in"
    }active when position is ${position}`,
    property,
    tagName: "div",
    rules: [`div { ${property}: ${value}; position: ${position}; }`],
    isActive,
  };
}

function makeTestCases() {
  const tests = [];

  for (const { property, value } of PROPERTIES) {
    for (const { position, isActive } of POSITIONS) {
      tests.push(makeTestCase(property, value, position, isActive));
    }
  }

  return tests;
}

export default makeTestCases();
