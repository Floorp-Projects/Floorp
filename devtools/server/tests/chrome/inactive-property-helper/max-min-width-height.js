/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `width`, `min-width`, `max-width`, `height`, `min-height`,
// `max-height` test cases.
export default [
  {
    info: "width is inactive on a non-replaced inline element",
    property: "width",
    tagName: "span",
    rules: ["span { width: 500px; }"],
    isActive: false,
  },
  {
    info: "min-width is inactive on a non-replaced inline element",
    property: "min-width",
    tagName: "span",
    rules: ["span { min-width: 500px; }"],
    isActive: false,
  },
  {
    info: "max-width is inactive on a non-replaced inline element",
    property: "max-width",
    tagName: "span",
    rules: ["span { max-width: 500px; }"],
    isActive: false,
  },
  {
    info: "width is inactive on an tr element",
    property: "width",
    tagName: "tr",
    rules: ["tr { width: 500px; }"],
    isActive: false,
  },
  {
    info: "min-width is inactive on an tr element",
    property: "min-width",
    tagName: "tr",
    rules: ["tr { min-width: 500px; }"],
    isActive: false,
  },
  {
    info: "max-width is inactive on an tr element",
    property: "max-width",
    tagName: "tr",
    rules: ["tr { max-width: 500px; }"],
    isActive: false,
  },
  {
    info: "width is inactive on an thead element",
    property: "width",
    tagName: "thead",
    rules: ["thead { width: 500px; }"],
    isActive: false,
  },
  {
    info: "min-width is inactive on an thead element",
    property: "min-width",
    tagName: "thead",
    rules: ["thead { min-width: 500px; }"],
    isActive: false,
  },
  {
    info: "max-width is inactive on an thead element",
    property: "max-width",
    tagName: "thead",
    rules: ["thead { max-width: 500px; }"],
    isActive: false,
  },
  {
    info: "width is inactive on an tfoot element",
    property: "width",
    tagName: "tfoot",
    rules: ["tfoot { width: 500px; }"],
    isActive: false,
  },
  {
    info: "min-width is inactive on an tfoot element",
    property: "min-width",
    tagName: "tfoot",
    rules: ["tfoot { min-width: 500px; }"],
    isActive: false,
  },
  {
    info: "max-width is inactive on an tfoot element",
    property: "max-width",
    tagName: "tfoot",
    rules: ["tfoot { max-width: 500px; }"],
    isActive: false,
  },
  {
    info: "width is active on a replaced inline element",
    property: "width",
    tagName: "img",
    rules: ["img { width: 500px; }"],
    isActive: true,
  },
  {
    info: "width is active on an inline input element",
    property: "width",
    tagName: "input",
    rules: ["input { display: inline; width: 500px; }"],
    isActive: true,
  },
  {
    info: "width is active on an inline select element",
    property: "width",
    tagName: "select",
    rules: ["select { display: inline; width: 500px; }"],
    isActive: true,
  },
  {
    info: "width is active on a textarea element",
    property: "width",
    tagName: "textarea",
    rules: ["textarea { width: 500px; }"],
    isActive: true,
  },
  {
    info: "min-width is active on a replaced inline element",
    property: "min-width",
    tagName: "img",
    rules: ["img { min-width: 500px; }"],
    isActive: true,
  },
  {
    info: "max-width is active on a replaced inline element",
    property: "max-width",
    tagName: "img",
    rules: ["img { max-width: 500px; }"],
    isActive: true,
  },
  {
    info: "width is active on a block element",
    property: "width",
    tagName: "div",
    rules: ["div { width: 500px; }"],
    isActive: true,
  },
  {
    info: "min-width is active on a block element",
    property: "min-width",
    tagName: "div",
    rules: ["div { min-width: 500px; }"],
    isActive: true,
  },
  {
    info: "max-width is active on a block element",
    property: "max-width",
    tagName: "div",
    rules: ["div { max-width: 500px; }"],
    isActive: true,
  },
  {
    info: "height is inactive on a non-replaced inline element",
    property: "height",
    tagName: "span",
    rules: ["span { height: 500px; }"],
    isActive: false,
  },
  {
    info: "min-height is inactive on a non-replaced inline element",
    property: "min-height",
    tagName: "span",
    rules: ["span { min-height: 500px; }"],
    isActive: false,
  },
  {
    info: "max-height is inactive on a non-replaced inline element",
    property: "max-height",
    tagName: "span",
    rules: ["span { max-height: 500px; }"],
    isActive: false,
  },
  {
    info: "height is inactive on colgroup element",
    property: "height",
    tagName: "colgroup",
    rules: ["colgroup { height: 500px; }"],
    isActive: false,
  },
  {
    info: "min-height is inactive on colgroup element",
    property: "min-height",
    tagName: "colgroup",
    rules: ["colgroup { min-height: 500px; }"],
    isActive: false,
  },
  {
    info: "max-height is inactive on colgroup element",
    property: "max-height",
    tagName: "colgroup",
    rules: ["colgroup { max-height: 500px; }"],
    isActive: false,
  },
  {
    info: "height is inactive on col element",
    property: "height",
    tagName: "col",
    rules: ["col { height: 500px; }"],
    isActive: false,
  },
  {
    info: "min-height is inactive on col element",
    property: "min-height",
    tagName: "col",
    rules: ["col { min-height: 500px; }"],
    isActive: false,
  },
  {
    info: "max-height is inactive on col element",
    property: "max-height",
    tagName: "col",
    rules: ["col { max-height: 500px; }"],
    isActive: false,
  },
  {
    info: "height is active on a replaced inline element",
    property: "height",
    tagName: "img",
    rules: ["img { height: 500px; }"],
    isActive: true,
  },
  {
    info: "height is active on an inline input element",
    property: "height",
    tagName: "input",
    rules: ["input { display: inline; height: 500px; }"],
    isActive: true,
  },
  {
    info: "height is active on an inline select element",
    property: "height",
    tagName: "select",
    rules: ["select { display: inline; height: 500px; }"],
    isActive: true,
  },
  {
    info: "height is active on a textarea element",
    property: "height",
    tagName: "textarea",
    rules: ["textarea { height: 500px; }"],
    isActive: true,
  },
  {
    info: "min-height is active on a replaced inline element",
    property: "min-height",
    tagName: "img",
    rules: ["img { min-height: 500px; }"],
    isActive: true,
  },
  {
    info: "max-height is active on a replaced inline element",
    property: "max-height",
    tagName: "img",
    rules: ["img { max-height: 500px; }"],
    isActive: true,
  },
  {
    info: "height is active on a block element",
    property: "height",
    tagName: "div",
    rules: ["div { height: 500px; }"],
    isActive: true,
  },
  {
    info: "min-height is active on a block element",
    property: "min-height",
    tagName: "div",
    rules: ["div { min-height: 500px; }"],
    isActive: true,
  },
  {
    info: "max-height is active on a block element",
    property: "max-height",
    tagName: "div",
    rules: ["div { max-height: 500px; }"],
    isActive: true,
  },
  {
    info: "height is active on an svg <rect> element.",
    property: "height",
    createTestElement: main => {
      main.innerHTML = `
        <svg width=100 height=100>
          <rect width=100 fill=green></rect>
        </svg>
      `;
      return main.querySelector("rect");
    },
    rules: ["rect { height: 100px; }"],
    isActive: true,
  },
  createTableElementTestCase("width", false, "table-row"),
  createTableElementTestCase("width", false, "table-row-group"),
  createTableElementTestCase("width", true, "table-column"),
  createTableElementTestCase("width", true, "table-column-group"),
  createTableElementTestCase("height", false, "table-column"),
  createTableElementTestCase("height", false, "table-column-group"),
  createTableElementTestCase("height", true, "table-row"),
  createTableElementTestCase("height", true, "table-row-group"),
  createVerticalTableElementTestCase("width", true, "table-row"),
  createVerticalTableElementTestCase("width", true, "table-row-group"),
  createVerticalTableElementTestCase("width", false, "table-column"),
  createVerticalTableElementTestCase("width", false, "table-column-group"),
  createVerticalTableElementTestCase("height", true, "table-column"),
  createVerticalTableElementTestCase("height", true, "table-column-group"),
  createVerticalTableElementTestCase("height", false, "table-row"),
  createVerticalTableElementTestCase("height", false, "table-row-group"),
  {
    info:
      "width's inactivity status for a row takes the table's writing mode into account",
    property: "width",
    createTestElement: rootNode => {
      const table = document.createElement("table");
      table.style.writingMode = "vertical-lr";
      rootNode.appendChild(table);

      const tbody = document.createElement("tbody");
      table.appendChild(tbody);

      const tr = document.createElement("tr");
      tbody.appendChild(tr);

      const td = document.createElement("td");
      tr.appendChild(td);

      return tr;
    },
    rules: ["tr { writing-mode: horizontal-tb; width: 360px; }"],
    isActive: true,
  },
];

function createTableElementTestCase(property, isActive, displayType) {
  return {
    info: `${property} is ${
      isActive ? "active" : "inactive"
    } on a ${displayType}`,
    property,
    tagName: "div",
    rules: [`div { display: ${displayType}; ${property}: 100px; }`],
    isActive,
  };
}

function createVerticalTableElementTestCase(property, isActive, displayType) {
  return {
    info: `${property} is ${
      isActive ? "active" : "inactive"
    } on a vertical ${displayType}`,
    property,
    createTestElement: rootNode => {
      const container = document.createElement("div");
      container.style.writingMode = "vertical-lr";
      rootNode.append(container);

      const element = document.createElement("span");
      container.append(element);

      return element;
    },
    rules: [`span { display: ${displayType}; ${property}: 100px; }`],
    isActive,
  };
}
