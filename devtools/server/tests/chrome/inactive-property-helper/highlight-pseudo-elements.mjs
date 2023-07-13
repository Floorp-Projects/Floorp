/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `highlight-pseudo-elements` test cases.

// "background",
// "background-color",
// "color",
// "fill-color",
// "stroke-color",
// "stroke-width",
// "text-decoration",
// "text-shadow",
// "text-underline-offset",
// "text-underline-position",

export default [
  {
    info: "width is inactive on ::selection",
    property: "width",
    tagName: "span",
    rules: ["span::selection { width: 10px; }"],
    isActive: false,
    // `width` is also inactive on inline element, so make sure we get the warning
    // because we're using it in a highlight pseudo.
    expectedMsgId: "inactive-css-highlight-pseudo-elements-not-supported",
  },
  {
    info: "display is inactive on ::highlight",
    property: "display",
    tagName: "span",
    rules: ["span::highlight(result) { display: grid; }"],
    isActive: false,
    expectedMsgId: "inactive-css-highlight-pseudo-elements-not-supported",
  },
  {
    // accept background shorthand, even if it might hold inactive values
    info: "background is active on ::selection",
    property: "background",
    tagName: "span",
    rules: ["span::selection { background: red; }"],
    isActive: true,
  },
  {
    info: "border-color is inactive on ::selection",
    property: "border-color",
    tagName: "span",
    rules: ["span::selection { border-color: red; }"],
    isActive: false,
    // `width` is also inactive on inline element, so make sure we get the warning
    // because we're using it in a highlight pseudo.
    expectedMsgId: "inactive-css-highlight-pseudo-elements-not-supported",
  },
  {
    info: "background-color is active on ::selection",
    property: "background-color",
    tagName: "span",
    rules: ["span::selection { background-color: red; }"],
    isActive: true,
  },
  {
    info: "color is active on ::selection",
    property: "color",
    tagName: "span",
    rules: ["span::selection { color: red; }"],
    isActive: true,
  },
  {
    info: "text-decoration is active on ::selection",
    property: "text-decoration",
    tagName: "span",
    rules: [
      "span::selection { text-decoration: double overline #FF3028 4px; }",
    ],
    isActive: true,
  },
  {
    info: "text-decoration-color is active on ::selection",
    property: "text-decoration-color",
    tagName: "span",
    rules: ["span::selection { text-decoration-color: #FF3028; }"],
    isActive: true,
  },
  {
    info: "text-decoration-line is active on ::selection",
    property: "text-decoration-line",
    tagName: "span",
    rules: ["span::selection { text-decoration-line: overline; }"],
    isActive: true,
  },
  {
    info: "text-decoration-style is active on ::selection",
    property: "text-decoration-style",
    tagName: "span",
    rules: ["span::selection { text-decoration-style: double; }"],
    isActive: true,
  },
  {
    info: "text-decoration-thickness is active on ::selection",
    property: "text-decoration-thickness",
    tagName: "span",
    rules: ["span::selection { text-decoration-thickness: 4px; }"],
    isActive: true,
  },
  {
    info: "text-shadow is active on ::selection",
    property: "text-shadow",
    tagName: "span",
    rules: ["span::selection { text-shadow: text-shadow: #FC0 1px 0 10px; }"],
    isActive: true,
  },
  {
    info: "text-underline-offset is active on ::selection",
    property: "text-underline-offset",
    tagName: "span",
    rules: ["span::selection { text-underline-offset: 10px; }"],
    isActive: true,
  },
  {
    info: "text-underline-position is active on ::selection",
    property: "text-underline-position",
    tagName: "span",
    rules: ["span::selection { text-underline-position: under; }"],
    isActive: true,
  },
  {
    info: "-webkit-text-fill-color is active on ::selection",
    property: "-webkit-text-fill-color",
    tagName: "span",
    rules: ["span::selection { -webkit-text-fill-color: red; }"],
    isActive: true,
  },
  {
    info: "-webkit-text-stroke-color is active on ::selection",
    property: "-webkit-text-stroke-color",
    tagName: "span",
    rules: ["span::selection { -webkit-text-stroke-color: red; }"],
    isActive: true,
  },
  {
    info: "-webkit-text-stroke-width is active on ::selection",
    property: "-webkit-text-stroke-width",
    tagName: "span",
    rules: ["span::selection { -webkit-text-stroke-width: 4px; }"],
    isActive: true,
  },
  {
    info: "-webkit-text-stroke is active on ::selection",
    property: "-webkit-text-stroke",
    tagName: "span",
    rules: ["span::selection { -webkit-text-stroke: 4px navy; }"],
    isActive: true,
  },
];
