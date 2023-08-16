/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `first-line-pseudo-element` test cases.

// "direction",
// "text-orientation",
// "writing-mode",

export default [
  {
    info: "direction is inactive on ::first-line",
    property: "direction",
    tagName: "div",
    rules: ["div::first-line { direction: rtl; }"],
    isActive: false,
    expectedMsgId: "inactive-css-first-line-pseudo-element-not-supported",
  },
  {
    info: "text-orientation is inactive on ::first-line",
    property: "text-orientation",
    tagName: "div",
    rules: ["div::first-line { text-orientation: sideways; }"],
    isActive: false,
    expectedMsgId: "inactive-css-first-line-pseudo-element-not-supported",
  },
  {
    info: "writing-mode is inactive on ::first-line",
    property: "writing-mode",
    tagName: "div",
    rules: ["div::first-line { writing-mode: vertical-rl; }"],
    isActive: false,
    expectedMsgId: "inactive-css-first-line-pseudo-element-not-supported",
  },
  {
    info: "color is active on ::first-line",
    property: "color",
    tagName: "div",
    rules: ["div::first-line { color: green; }"],
    isActive: true,
  },
  {
    info: "display is active on ::first-line",
    property: "display",
    tagName: "div",
    rules: ["div::first-line { display: grid; }"],
    isActive: true,
  },
];
