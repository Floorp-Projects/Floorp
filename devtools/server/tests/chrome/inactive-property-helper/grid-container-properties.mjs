/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper test cases:
// `grid-auto-columns`, `grid-auto-flow`, `grid-auto-rows`, `grid-template`,
// `grid-template-areas`, `grid-template-columns`, `grid-template-rows`,
// and `justify-items`.
let tests = [];

for (const { propertyName, propertyValue } of [
  { propertyName: "grid-auto-columns", propertyValue: "100px" },
  { propertyName: "grid-auto-flow", propertyValue: "columns" },
  { propertyName: "grid-auto-rows", propertyValue: "100px" },
  { propertyName: "grid-template", propertyValue: "auto / auto" },
  { propertyName: "grid-template-areas", propertyValue: "a b c" },
  { propertyName: "grid-template-columns", propertyValue: "100px 1fr" },
  { propertyName: "grid-template-rows", propertyValue: "100px 1fr" },
  { propertyName: "justify-items", propertyValue: "center" },
]) {
  tests = tests.concat(createTestsForProp(propertyName, propertyValue));
}

function createTestsForProp(propertyName, propertyValue) {
  return [
    {
      info: `${propertyName} is active on a grid container`,
      property: propertyName,
      tagName: "div",
      rules: [`div { display:grid; ${propertyName}: ${propertyValue}; }`],
      isActive: true,
    },
    {
      info: `${propertyName} is inactive on a non-grid container`,
      property: propertyName,
      tagName: "div",
      rules: [`div { ${propertyName}: ${propertyValue}; }`],
      isActive: false,
    },
  ];
}

export default tests;
