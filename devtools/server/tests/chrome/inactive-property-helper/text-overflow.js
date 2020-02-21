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
    info: "text-overflow is inactive when overflow is set to visible",
    property: "text-overflow",
    tagName: "div",
    rules: ["div { text-overflow: ellipsis; overflow: visible; }"],
    isActive: false,
  },
  {
    info:
      "as soon as overflow:hidden is set, text-overflow is active whatever the box type",
    property: "text-overflow",
    tagName: "span",
    rules: ["span { text-overflow: ellipsis; overflow: hidden; }"],
    isActive: true,
  },
  {
    info:
      "as soon as overflow:hidden is set, text-overflow is active whatever the box type",
    property: "text-overflow",
    tagName: "legend",
    rules: ["legend { text-overflow: ellipsis; overflow: hidden; }"],
    isActive: true,
  },
];
