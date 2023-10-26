/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `text-wrap: balance` test cases.
const LOREM_IPSUM = `
  Lorem ipsum dolor sit amet, consectetur adipiscing elit.
  Donec a diam lectus. Sed sit amet ipsum mauris.
  Maecenas congue ligula ac quam viverra nec consectetur ante hendrerit.
`;

export default [
  {
    info: "text-wrap: balance; is inactive when line number exceeds threshold",
    property: "text-wrap",
    createTestElement: rootNode => {
      const element = document.createElement("div");
      element.textContent = LOREM_IPSUM;
      rootNode.append(element);
      return element;
    },
    tagName: "div",
    rules: ["div { text-wrap: balance; width: 100px; }"],
    isActive: false,
  },
  {
    info: "text-wrap: balance; is active when line number is below threshold",
    property: "text-wrap",
    createTestElement: rootNode => {
      const element = document.createElement("div");
      element.textContent = LOREM_IPSUM;
      rootNode.append(element);
      return element;
    },
    tagName: "div",
    rules: ["div { text-wrap: balance; width: 300px; }"],
    isActive: true,
  },
  {
    info: "text-wrap: balance is inactive when element is fragmented",
    property: "text-wrap",
    createTestElement: rootNode => {
      const element = document.createElement("div");
      element.textContent = `
        Lorem ipsum dolor sit amet, consectetur adipiscing elit.
        Donec a diam lectus. Sed sit amet ipsum mauris.
        Maecenas congue ligula ac quam viverra nec consectetur ante hendrerit.
      `;
      rootNode.append(element);
      return element;
    },
    tagName: "div",
    rules: ["div { text-wrap: balance; column-count: 2; }"],
    isActive: false,
  },
  {
    info: "text-wrap: balance; does not throw if element is not a block",
    property: "text-wrap",
    createTestElement: rootNode => {
      const element = document.createElement("div");
      element.textContent = LOREM_IPSUM;
      rootNode.append(element);
      return element;
    },
    tagName: "div",
    rules: ["div { text-wrap: balance; display: inline; }"],
    isActive: true,
  },
  {
    info: "text-wrap: initial; is active",
    property: "text-wrap",
    createTestElement: rootNode => {
      const element = document.createElement("div");
      element.textContent = `
        Lorem ipsum dolor sit amet, consectetur adipiscing elit.
        Donec a diam lectus. Sed sit amet ipsum mauris.
        Maecenas congue ligula ac quam viverra nec consectetur ante hendrerit.
      `;
      rootNode.append(element);
      return element;
    },
    tagName: "div",
    rules: ["div { text-wrap: initial; width: 100px; }"],
    isActive: true,
  },
];
