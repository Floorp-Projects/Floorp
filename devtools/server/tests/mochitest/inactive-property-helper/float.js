/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `float` test cases.
export default [
  {
    info: "display: inline is inactive on a floated element",
    property: "display",
    tagName: "div",
    rules: ["div { display: inline; float: right; }"],
    isActive: false,
  },
  {
    info: "display: block is active on a floated element",
    property: "display",
    tagName: "div",
    rules: ["div { display: block; float: right;}"],
    isActive: true,
  },
  {
    info: "display: inline-grid is inactive on a floated element",
    property: "display",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      container.classList.add("test");
      rootNode.append(container);
      return container;
    },
    rules: [
      "div { float: left; display:block; }",
      ".test { display: inline-grid ;}",
    ],
    isActive: false,
  },
  {
    info: "display: table-footer-group is inactive on a floated element",
    property: "display",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      container.style.display = "table";
      const footer = document.createElement("div");
      footer.classList.add("table-footer");
      container.append(footer);
      rootNode.append(container);
      return footer;
    },
    rules: [".table-footer { display: table-footer-group; float: left;}"],
    isActive: false,
  },
];
