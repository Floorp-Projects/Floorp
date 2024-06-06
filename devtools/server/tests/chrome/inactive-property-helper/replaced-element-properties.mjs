/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `object-fit` and `object-position` test cases.
export default [
  {
    info: "object-fit is inactive on inline element",
    property: "object-fit",
    tagName: "span",
    rules: ["span { object-fit: cover; }"],
    isActive: false,
  },
  {
    info: "object-fit is active on replaced element",
    property: "object-fit",
    tagName: "img",
    rules: ["img { object-fit: cover; }"],
    isActive: true,
  },
  {
    info: "object-position is inactive on inline element",
    property: "object-position",
    tagName: "span",
    rules: ["span { object-position: 50% 50%; }"],
    isActive: false,
  },
  {
    info: "object-position is active on replaced element",
    property: "object-position",
    tagName: "img",
    rules: ["img { object-position: 50% 50%; }"],
    isActive: true,
  },
];
