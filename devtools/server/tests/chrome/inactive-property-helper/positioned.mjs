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

// Make the test cases for all the combinations of PROPERTIES and POSITIONS
const mainTests = [];

for (const { property, value } of PROPERTIES) {
  for (const { position, isActive } of POSITIONS) {
    mainTests.push(makeTestCase(property, value, position, isActive));
  }
}

// Add a few test cases to check that z-index actually works inside grids and flexboxes.
mainTests.push({
  info: "z-index is active even on unpositioned elements if they are grid items",
  property: "z-index",
  createTestElement: rootNode => {
    const container = document.createElement("div");
    const element = document.createElement("span");
    container.append(element);
    rootNode.append(container);
    return element;
  },
  rules: ["div { display: grid; }", "span { z-index: 3; }"],
  ruleIndex: 1,
  isActive: true,
});

mainTests.push({
  info: "z-index is active even on unpositioned elements if they are flex items",
  property: "z-index",
  createTestElement: rootNode => {
    const container = document.createElement("div");
    const element = document.createElement("span");
    container.append(element);
    rootNode.append(container);
    return element;
  },
  rules: ["div { display: flex; }", "span { z-index: 3; }"],
  ruleIndex: 1,
  isActive: true,
});

export default mainTests;
