/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `empty-cells` test cases.
export default [
  {
    info: "empty-cells is inactive on block element",
    property: "empty-cells",
    tagName: "div",
    rules: ["div { empty-cells: hide; }"],
    isActive: false,
  },
  {
    info: "empty-cells is active on table cell element",
    property: "empty-cells",
    tagName: "div",
    rules: ["div { display: table-cell; empty-cells: hide; }"],
    isActive: true,
  },
];
