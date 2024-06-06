/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper: `column-span`  test cases.
const tests = [
  {
    info: `column-span is active on an element within a multi-column container established by columns property`,
    property: "column-span",
    createTestElement,
    tagName: "div",
    rules: [
      `#multicol-container { columns:2; }`,
      `#multicol-item { column-span: all; }`,
    ],
    isActive: true,
  },
  {
    info: `column-span is active on an element within a multi-column container established by column-count property`,
    property: "column-span",
    createTestElement,
    tagName: "div",
    rules: [
      `#multicol-container { column-count: 2; }`,
      `#multicol-item { column-span: all; }`,
    ],
    isActive: true,
  },
  {
    info: `column-span is active on an element within a multi-column container established by column-width property`,
    property: "column-span",
    createTestElement,
    tagName: "div",
    rules: [
      `#multicol-container { column-width: 100px; }`,
      `#multicol-item { column-span: all; }`,
    ],
    isActive: true,
  },
  {
    info: `column-span is inactive on an element outside a multi-column container`,
    property: "column-span",
    createTestElement,
    tagName: "div",
    rules: [`#multicol-item { column-span: all; }`],
    isActive: false,
  },
];

function createTestElement(rootNode) {
  const container = document.createElement("div");
  container.id = "multicol-container";
  const wrapper = document.createElement("div");
  const element = document.createElement("div");
  element.id = "multicol-item";
  wrapper.append(element);
  container.append(wrapper);
  rootNode.append(container);

  return element;
}

export default tests;
