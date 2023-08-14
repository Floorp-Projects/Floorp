/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper test cases:
// `column-fill`, `column-rule`, `column-rule-color`, `column-rule-style`,
// and `column-rule-width`.
let tests = [];

for (const { propertyName, propertyValue } of [
  { propertyName: "column-fill", propertyValue: "auto" },
  { propertyName: "column-rule", propertyValue: "1px solid black" },
  { propertyName: "column-rule-color", propertyValue: "black" },
  { propertyName: "column-rule-style", propertyValue: "solid" },
  { propertyName: "column-rule-width", propertyValue: "1px" },
]) {
  tests = tests.concat(createTestsForProp(propertyName, propertyValue));
}

function createTestsForProp(propertyName, propertyValue) {
  return [
    {
      info: `${propertyName} is active on a multi-column container`,
      property: propertyName,
      tagName: "div",
      rules: [`div { columns:2; ${propertyName}: ${propertyValue}; }`],
      isActive: true,
    },
    {
      info: `${propertyName} is inactive on a non-multi-column container`,
      property: propertyName,
      tagName: "div",
      rules: [`div { ${propertyName}: ${propertyValue}; }`],
      isActive: false,
    },
  ];
}

export default tests;
