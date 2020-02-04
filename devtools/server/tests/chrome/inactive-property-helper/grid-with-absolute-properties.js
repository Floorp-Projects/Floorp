/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper test cases:
// `grid-area`, `grid-column`, `grid-column-end`, `grid-column-start`,
// `grid-row`, `grid-row-end`, `grid-row-start`, `justify-self`, `align-self`
// and `place-self`.
let tests = [];

for (const { propertyName, propertyValue } of [
  { propertyName: "grid-area", propertyValue: "2 / 1 / span 2 / span 3" },
  { propertyName: "grid-column", propertyValue: 2 },
  { propertyName: "grid-column-end", propertyValue: "span 3" },
  { propertyName: "grid-column-start", propertyValue: 2 },
  { propertyName: "grid-row", propertyValue: "1 / span 2" },
  { propertyName: "grid-row-end", propertyValue: "span 3" },
  { propertyName: "grid-row-start", propertyValue: 2 },
  { propertyName: "justify-self", propertyValue: "start" },
  { propertyName: "align-self", propertyValue: "auto" },
  { propertyName: "place-self", propertyValue: "auto center" },
]) {
  tests = tests.concat(createTestsForProp(propertyName, propertyValue));
}

function createTestsForProp(propertyName, propertyValue) {
  return [
    {
      info: `${propertyName} is active on a grid item`,
      property: `${propertyName}`,
      createTestElement,
      rules: [
        `#grid-container { display:grid; grid:auto/100px 100px; }`,
        `#grid-item { ${propertyName}: ${propertyValue}; }`,
      ],
      ruleIndex: 1,
      isActive: true,
    },
    {
      info: `${propertyName} is active on an absolutely positioned grid item`,
      property: `${propertyName}`,
      createTestElement,
      rules: [
        `#grid-container { display:grid; grid:auto/100px 100px; position: relative }`,
        `#grid-item { ${propertyName}: ${propertyValue}; position: absolute; }`,
      ],
      ruleIndex: 1,
      isActive: true,
    },
    {
      info: `${propertyName} is inactive on a non-grid item`,
      property: `${propertyName}`,
      tagName: `div`,
      rules: [`div { ${propertyName}: ${propertyValue}; }`],
      isActive: false,
    },
  ];
}

function createTestElement(rootNode) {
  const container = document.createElement("div");
  container.id = "grid-container";
  const element = document.createElement("div");
  element.id = "grid-item";
  container.append(element);
  rootNode.append(container);

  return element;
}

export default tests;
