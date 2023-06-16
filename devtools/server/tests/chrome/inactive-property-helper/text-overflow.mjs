/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `text-overflow` test cases.
export default [
  {
    info: "text-overflow is inactive when overflow is not set",
    property: "text-overflow",
    tagName: "div",
    rules: ["div { text-overflow: ellipsis; }"],
    isActive: false,
  },
  {
    info: "text-overflow is active when overflow is set to hidden",
    property: "text-overflow",
    tagName: "div",
    rules: ["div { text-overflow: ellipsis; overflow: hidden; }"],
    isActive: true,
  },
  {
    info: "text-overflow is active when overflow is set to auto",
    property: "text-overflow",
    tagName: "div",
    rules: ["div { text-overflow: ellipsis; overflow: auto; }"],
    isActive: true,
  },
  {
    info: "text-overflow is active when overflow is set to scroll",
    property: "text-overflow",
    tagName: "div",
    rules: ["div { text-overflow: ellipsis; overflow: scroll; }"],
    isActive: true,
  },
  {
    info: "text-overflow is inactive when overflow is set to visible",
    property: "text-overflow",
    tagName: "div",
    rules: ["div { text-overflow: ellipsis; overflow: visible; }"],
    isActive: false,
  },
  {
    info: "text-overflow is active when overflow-x is set to hidden on horizontal writing mode",
    property: "text-overflow",
    tagName: "div",
    rules: [
      "div { writing-mode: lr; text-overflow: ellipsis; overflow-x: hidden; }",
    ],
    isActive: true,
  },
  {
    info: "text-overflow is inactive when overflow-x is set to visible on horizontal writing mode",
    property: "text-overflow",
    tagName: "div",
    rules: [
      "div { writing-mode: lr; text-overflow: ellipsis; overflow-x: visible; }",
    ],
    isActive: false,
  },
  {
    info: "text-overflow is active when overflow-y is set to hidden on vertical writing mode",
    property: "text-overflow",
    tagName: "div",
    rules: [
      "div { writing-mode: vertical-lr; text-overflow: ellipsis; overflow-y: hidden; }",
    ],
    isActive: true,
  },
  {
    info: "text-overflow is inactive when overflow-y is set to visible on vertical writing mode",
    property: "text-overflow",
    tagName: "div",
    rules: [
      "div { writing-mode: vertical-lr; text-overflow: ellipsis; overflow-y: visible; }",
    ],
    isActive: false,
  },
  {
    info: "as soon as overflow:hidden is set, text-overflow is active whatever the box type",
    property: "text-overflow",
    tagName: "span",
    rules: ["span { text-overflow: ellipsis; overflow: hidden; }"],
    isActive: true,
  },
  {
    info: "as soon as overflow:hidden is set, text-overflow is active whatever the box type",
    property: "text-overflow",
    tagName: "legend",
    rules: ["legend { text-overflow: ellipsis; overflow: hidden; }"],
    isActive: true,
  },
];
