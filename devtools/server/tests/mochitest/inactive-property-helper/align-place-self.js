/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `align-self` and `place-self` test cases.
export default [
  {
    info: "align-self is inactive on block element",
    property: "align-self",
    tagName: "div",
    rules: ["div { align-self: center; }"],
    isActive: false,
  },
  {
    info: "align-self is inactive on flex container",
    property: "align-self",
    tagName: "div",
    rules: ["div { align-self: center;  display: flex;}"],
    isActive: false,
  },
  {
    info: "align-self is inactive on inline-flex container",
    property: "align-self",
    tagName: "div",
    rules: ["div { align-self: center;  display: inline-flex;}"],
    isActive: false,
  },
  {
    info: "align-self is inactive on grid container",
    property: "align-self",
    tagName: "div",
    rules: ["div { align-self: center;  display: grid;}"],
    isActive: false,
  },
  {
    info: "align-self is inactive on inline grid container",
    property: "align-self",
    tagName: "div",
    rules: ["div { align-self: center;  display: inline-grid;}"],
    isActive: false,
  },
  {
    info: "align-self is inactive on inline element",
    property: "align-self",
    tagName: "span",
    rules: ["span { align-self: center; }"],
    isActive: false,
  },
  {
    info: "align-self is active on flex item",
    property: "align-self",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      const element = document.createElement("span");
      container.append(element);
      rootNode.append(container);
      return element;
    },
    rules: [
      "div { display: flex; align-items: start; }",
      "span { align-self: center; }",
    ],
    ruleIndex: 1,
    isActive: true,
  },
  {
    info: "align-self is active on grid item",
    property: "align-self",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      const element = document.createElement("span");
      container.append(element);
      rootNode.append(container);
      return element;
    },
    rules: [
      "div { display: grid; align-items: start; }",
      "span { align-self: center; }",
    ],
    ruleIndex: 1,
    isActive: true,
  },
  {
    info: "place-self is inactive on block element",
    property: "place-self",
    tagName: "div",
    rules: ["div { place-self: center; }"],
    isActive: false,
  },
  {
    info: "place-self is inactive on flex container",
    property: "place-self",
    tagName: "div",
    rules: ["div { place-self: center;  display: flex;}"],
    isActive: false,
  },
  {
    info: "place-self is inactive on inline-flex container",
    property: "place-self",
    tagName: "div",
    rules: ["div { place-self: center;  display: inline-flex;}"],
    isActive: false,
  },
  {
    info: "place-self is inactive on grid container",
    property: "place-self",
    tagName: "div",
    rules: ["div { place-self: center;  display: grid;}"],
    isActive: false,
  },
  {
    info: "place-self is inactive on inline grid container",
    property: "place-self",
    tagName: "div",
    rules: ["div { place-self: center;  display: inline-grid;}"],
    isActive: false,
  },
  {
    info: "place-self is inactive on inline element",
    property: "place-self",
    tagName: "span",
    rules: ["span { place-self: center; }"],
    isActive: false,
  },
  {
    info: "place-self is active on flex item",
    property: "place-self",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      const element = document.createElement("span");
      container.append(element);
      rootNode.append(container);
      return element;
    },
    rules: [
      "div { display: flex; align-items: start; }",
      "span { place-self: center; }",
    ],
    ruleIndex: 1,
    isActive: true,
  },
  {
    info: "place-self is active on grid item",
    property: "place-self",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      const element = document.createElement("span");
      container.append(element);
      rootNode.append(container);
      return element;
    },
    rules: [
      "div { display: grid; align-items: start; }",
      "span { place-self: center; }",
    ],
    ruleIndex: 1,
    isActive: true,
  },
];
