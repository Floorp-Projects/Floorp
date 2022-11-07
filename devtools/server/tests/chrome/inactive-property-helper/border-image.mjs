/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `border-image` test cases.
export default [
  {
    info:
      "border-image is active on another element then a table element or internal table element where border-collapse is not set to collapse",
    property: "border-image",
    tagName: "div",
    rules: ["div { border-image: linear-gradient(red, yellow) 10; }"],
    isActive: true,
  },
  {
    info:
      "border-image is active on another element then a table element or internal table element where border-collapse is set to collapse",
    property: "border-image",
    tagName: "div",
    rules: [
      "div { border-image: linear-gradient(red, yellow) 10; border-collapse: collapse;}",
    ],
    isActive: true,
  },
  {
    info:
      "border-image is active on a td element with no table parent and the browser is not crashing",
    property: "border-image",
    tagName: "td",
    rules: [
      "td { border-image: linear-gradient(red, yellow) 10; border-collapse: collapse;}",
    ],
    isActive: true,
  },
  createTableElementsToTestBorderImage({
    useDivTagWithDisplayTableStyle: false,
    borderCollapse: true,
    borderCollapsePropertyIsInherited: false,
    isActive: true,
  }),
  createTableElementsToTestBorderImage({
    useDivTagWithDisplayTableStyle: false,
    borderCollapse: false,
    borderCollapsePropertyIsInherited: false,
    isActive: true,
  }),
  createTableElementsToTestBorderImage({
    useDivTagWithDisplayTableStyle: false,
    borderCollapse: true,
    borderCollapsePropertyIsInherited: true,
    isActive: false,
  }),
  createTableElementsToTestBorderImage({
    useDivTagWithDisplayTableStyle: false,
    borderCollapse: false,
    borderCollapsePropertyIsInherited: true,
    isActive: true,
  }),
  createTableElementsToTestBorderImage({
    useDivTagWithDisplayTableStyle: true,
    borderCollapse: true,
    borderCollapsePropertyIsInherited: false,
    isActive: true,
  }),
  createTableElementsToTestBorderImage({
    useDivTagWithDisplayTableStyle: true,
    borderCollapse: false,
    borderCollapsePropertyIsInherited: false,
    isActive: true,
  }),
  createTableElementsToTestBorderImage({
    useDivTagWithDisplayTableStyle: true,
    borderCollapse: true,
    borderCollapsePropertyIsInherited: true,
    isActive: false,
  }),
  createTableElementsToTestBorderImage({
    useDivTagWithDisplayTableStyle: true,
    borderCollapse: false,
    borderCollapsePropertyIsInherited: true,
    isActive: true,
  }),
];

/**
 * @param {Object} testParameters
 * @param {bool} testParameters.useDivTagWithDisplayTableStyle use generic divs using display property instead of actual table/tr/td tags
 * @param {bool} testParameters.borderCollapse is `border-collapse` property set to `collapse` ( instead of `separate`)
 * @param {bool} testParameters.borderCollapsePropertyIsInherited should the border collapse property be inherited from the table parent (instead of directly set on the internal table element)
 * @param {bool} testParameters.isActive is the border-image property actve on the element
 * @returns
 */
function createTableElementsToTestBorderImage({
  useDivTagWithDisplayTableStyle,
  borderCollapse,
  borderCollapsePropertyIsInherited,
  isActive,
}) {
  return {
    info: `border-image is ${
      isActive ? "active" : "inactive"
    } on an internal table element where border-collapse is${
      borderCollapse ? "" : " not"
    } set to collapse${
      borderCollapsePropertyIsInherited
        ? " by being inherited from its table parent"
        : ""
    } when the table and its internal elements are ${
      useDivTagWithDisplayTableStyle ? "not " : ""
    }using semantic tags (table, tr, td, ...)`,
    property: "border-image",
    createTestElement: rootNode => {
      const table = useDivTagWithDisplayTableStyle
        ? document.createElement("div")
        : document.createElement("table");
      if (useDivTagWithDisplayTableStyle) {
        table.style.display = "table";
      }
      if (borderCollapsePropertyIsInherited) {
        table.style.borderCollapse = `${
          borderCollapse ? "collapse" : "separate"
        }`;
      }
      rootNode.appendChild(table);

      const tbody = useDivTagWithDisplayTableStyle
        ? document.createElement("div")
        : document.createElement("tbody");
      if (useDivTagWithDisplayTableStyle) {
        tbody.style.display = "table-row-group";
      }
      table.appendChild(tbody);

      const tr = useDivTagWithDisplayTableStyle
        ? document.createElement("div")
        : document.createElement("tr");
      if (useDivTagWithDisplayTableStyle) {
        tr.style.display = "table-row";
      }
      tbody.appendChild(tr);

      const td = useDivTagWithDisplayTableStyle
        ? document.createElement("div")
        : document.createElement("td");
      if (useDivTagWithDisplayTableStyle) {
        td.style.display = "table-cell";
        td.classList.add("td");
      }
      tr.appendChild(td);

      return td;
    },
    rules: [
      `td, .td {
        border-image: linear-gradient(red, yellow) 10;
        ${
          !borderCollapsePropertyIsInherited
            ? `border-collapse: ${borderCollapse ? "collapse" : "separate"};`
            : ""
        }
     }`,
    ],
    isActive,
  };
}
