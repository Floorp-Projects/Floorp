/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `scroll-padding-*` test cases.

export default [
  {
    info: "scroll-padding is active on element with auto-overflow",
    property: "scroll-padding",
    tagName: "div",
    rules: ["div { overflow: auto; scroll-padding: 10px; }"],
    isActive: true,
  },
  {
    info: "scroll-padding is active on element with scrollable overflow",
    property: "scroll-padding",
    tagName: "div",
    rules: ["div { overflow: scroll; scroll-padding: 10px; }"],
    isActive: true,
  },
  {
    info: "scroll-padding is active on element with hidden overflow",
    property: "scroll-padding",
    tagName: "div",
    rules: ["div { overflow: hidden; scroll-padding: 10px; }"],
    isActive: true,
  },
  {
    info: "scroll-padding is inactive on element with visible overflow",
    property: "scroll-padding",
    tagName: "div",
    rules: ["div { scroll-padding: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding is inactive on element with clipped overflow",
    property: "scroll-padding",
    tagName: "div",
    rules: ["div { overflow: clip; scroll-padding: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding is inactive on element with horizontally clipped overflow",
    property: "scroll-padding",
    tagName: "div",
    rules: ["div { overflow-x: clip; scroll-padding: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding is inactive on element with vertically clipped overflow",
    property: "scroll-padding",
    tagName: "div",
    rules: ["div { overflow-y: clip; scroll-padding: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding-top is inactive on element with visible overflow",
    property: "scroll-padding-top",
    tagName: "div",
    rules: ["div { scroll-padding-top: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding-top is inactive on element with horizontally clipped overflow",
    property: "scroll-padding-top",
    tagName: "div",
    rules: ["div { overflow-x: clip; scroll-padding-top: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding-top is inactive on element with vertically clipped overflow",
    property: "scroll-padding-top",
    tagName: "div",
    rules: ["div { overflow-y: clip; scroll-padding-top: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding-top is active on element with horizontally clipped but vertical auto-overflow (as 'clip' is computed to 'hidden')",
    property: "scroll-padding-top",
    tagName: "div",
    rules: [
      "div { overflow-x: clip; overflow-y: auto; scroll-padding-top: 10px; }",
    ],
    isActive: true,
  },
  {
    info: "scroll-padding-top is active on element with vertically clipped but horizontal auto-overflow (as 'clip' is computed to 'hidden')",
    property: "scroll-padding-top",
    tagName: "div",
    rules: [
      "div { overflow-x: auto; overflow-y: clip; scroll-padding-top: 10px; }",
    ],
    isActive: true,
  },
  {
    info: "scroll-padding-right is inactive on element with visible overflow",
    property: "scroll-padding-right",
    tagName: "div",
    rules: ["div { scroll-padding-right: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding-bottom is inactive on element with visible overflow",
    property: "scroll-padding-bottom",
    tagName: "div",
    rules: ["div { scroll-padding-bottom: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding-left is inactive on element with visible overflow",
    property: "scroll-padding-left",
    tagName: "div",
    rules: ["div { scroll-padding-left: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding-block is inactive on element with visible overflow",
    property: "scroll-padding-block",
    tagName: "div",
    rules: ["div { scroll-padding-block: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding-block-end is inactive on element with visible overflow",
    property: "scroll-padding-block-end",
    tagName: "div",
    rules: ["div { scroll-padding-block-end: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding-block-start is inactive on element with visible overflow",
    property: "scroll-padding-block-start",
    tagName: "div",
    rules: ["div { scroll-padding-block-start: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding-inline is inactive on element with visible overflow",
    property: "scroll-padding-inline",
    tagName: "div",
    rules: ["div { scroll-padding-inline: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding-inline-end is inactive on element with visible overflow",
    property: "scroll-padding-inline-end",
    tagName: "div",
    rules: ["div { scroll-padding-inline-end: 10px; }"],
    isActive: false,
  },
  {
    info: "scroll-padding-inline-start is inactive on element with visible overflow",
    property: "scroll-padding-inline-start",
    tagName: "div",
    rules: ["div { scroll-padding-inline-start: 10px; }"],
    isActive: false,
  },
];
