/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `first-letter-pseudo-element` test cases.

// "content",

export default [
  {
    info: "content is inactive on ::first-letter",
    property: "content",
    tagName: "div",
    rules: ["div::first-letter { content: 'invalid'; }"],
    isActive: false,
    expectedMsgId: "inactive-css-first-letter-pseudo-element-not-supported",
  },
  {
    info: "color is active on ::first-letter",
    property: "color",
    tagName: "div",
    rules: ["div::first-letter { color: green; }"],
    isActive: true,
  },
  {
    info: "display is active on ::first-letter",
    property: "display",
    tagName: "div",
    rules: ["div::first-letter { display: grid; }"],
    isActive: true,
  },
];
