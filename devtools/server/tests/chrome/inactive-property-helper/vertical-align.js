/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `vertical-align` test cases.
export default [
  {
    info: "vertical-align is inactive on a block element",
    property: "vertical-align",
    tagName: "div",
    rules: ["div { vertical-align: top; }"],
    isActive: false,
  },
  {
    info: "vertical-align is inactive on a span with display block",
    property: "vertical-align",
    tagName: "span",
    rules: ["span { vertical-align: top; display: block;}"],
    isActive: false,
  },
  {
    info: "vertical-align is active on a div with display inline-block",
    property: "vertical-align",
    tagName: "div",
    rules: ["div { vertical-align: top; display: inline-block;}"],
    isActive: true,
  },
  {
    info: "vertical-align is active on a table-cell",
    property: "vertical-align",
    tagName: "div",
    rules: ["div { vertical-align: top; display: table-cell;}"],
    isActive: true,
  },
  {
    info: "vertical-align is active on a block element ::first-letter",
    property: "vertical-align",
    tagName: "div",
    rules: ["div::first-letter { vertical-align: top; }"],
    isActive: true,
  },
  {
    info: "vertical-align is active on a block element ::first-line",
    property: "vertical-align",
    tagName: "div",
    rules: ["div::first-line { vertical-align: top; }"],
    isActive: true,
  },
  {
    info: "vertical-align is active on an inline element",
    property: "vertical-align",
    tagName: "span",
    rules: ["span { vertical-align: top; }"],
    isActive: true,
  },
];
