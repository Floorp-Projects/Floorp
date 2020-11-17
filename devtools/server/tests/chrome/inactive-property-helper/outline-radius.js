/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `outline-radius` test cases.
export default [
  {
    info: "-moz-outline-radius is inactive when outline-style is auto",
    property: "-moz-outline-radius",
    tagName: "div",
    rules: ["div { outline: 10px auto; -moz-outline-radius: 10px; }"],
    isActive: false,
  },
  {
    info: "-moz-outline-radius is inactive when outline-style is none",
    property: "-moz-outline-radius",
    tagName: "div",
    rules: ["div { -moz-outline-radius: 10px; }"],
    isActive: false,
  },
  {
    info: "-moz-outline-radius is active when outline-style is not auto",
    property: "-moz-outline-radius",
    tagName: "div",
    rules: ["div { outline: 10px solid; -moz-outline-radius: 10px; }"],
    isActive: true,
  },
];
