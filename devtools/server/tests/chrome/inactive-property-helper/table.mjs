/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `table-layout` test cases.
export default [
  {
    info: "table-layout is inactive on block element",
    property: "table-layout",
    tagName: "div",
    rules: ["div { table-layout: fixed; }"],
    isActive: false,
  },
  {
    info: "table-layout is active on table element",
    property: "table-layout",
    tagName: "div",
    rules: ["div { display: table; table-layout: fixed; }"],
    isActive: true,
  },
  {
    info: "table-layout is active on inline table element",
    property: "table-layout",
    tagName: "div",
    rules: ["div { display: inline-table; table-layout: fixed; }"],
    isActive: true,
  },
];
