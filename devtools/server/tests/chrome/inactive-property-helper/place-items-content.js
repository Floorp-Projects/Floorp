/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `place-items` and `place-content` test cases.
export default [
  {
    info: "place-items is inactive on block element",
    property: "place-items",
    tagName: "div",
    rules: ["div { place-items: center; }"],
    isActive: false,
  },
  {
    info: "place-items is inactive on inline element",
    property: "place-items",
    tagName: "span",
    rules: ["span { place-items: center; }"],
    isActive: false,
  },
  {
    info: "place-items is inactive on flex item",
    property: "place-items",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      const element = document.createElement("span");
      container.append(element);
      rootNode.append(container);
      return element;
    },
    rules: [
      "div { display: flex; align-items: start; }",
      "span { place-items: center; }",
    ],
    ruleIndex: 1,
    isActive: false,
  },
  {
    info: "place-items is inactive on grid item",
    property: "place-items",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      const element = document.createElement("span");
      container.append(element);
      rootNode.append(container);
      return element;
    },
    rules: [
      "div { display: grid; align-items: start; }",
      "span { place-items: center; }",
    ],
    ruleIndex: 1,
    isActive: false,
  },
  {
    info: "place-items is active on flex container",
    property: "place-items",
    tagName: "div",
    rules: ["div { place-items: center;  display: flex;}"],
    isActive: true,
  },
  {
    info: "place-items is active on inline-flex container",
    property: "place-items",
    tagName: "div",
    rules: ["div { place-items: center;  display: inline-flex;}"],
    isActive: true,
  },
  {
    info: "place-items is active on grid container",
    property: "place-items",
    tagName: "div",
    rules: ["div { place-items: center;  display: grid;}"],
    isActive: true,
  },
  {
    info: "place-items is active on inline grid container",
    property: "place-items",
    tagName: "div",
    rules: ["div { place-items: center;  display: inline-grid;}"],
    isActive: true,
  },
  {
    info: "place-content is inactive on block element",
    property: "place-content",
    tagName: "div",
    rules: ["div { place-content: center; }"],
    isActive: false,
  },
  {
    info: "place-content is inactive on inline element",
    property: "place-content",
    tagName: "span",
    rules: ["span { place-content: center; }"],
    isActive: false,
  },
  {
    info: "place-content is inactive on flex item",
    property: "place-content",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      const element = document.createElement("span");
      container.append(element);
      rootNode.append(container);
      return element;
    },
    rules: [
      "div { display: flex; align-items: start; }",
      "span { place-content: center; }",
    ],
    ruleIndex: 1,
    isActive: false,
  },
  {
    info: "place-content is inactive on grid item",
    property: "place-content",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      const element = document.createElement("span");
      container.append(element);
      rootNode.append(container);
      return element;
    },
    rules: [
      "div { display: grid; align-items: start; }",
      "span { place-content: center; }",
    ],
    ruleIndex: 1,
    isActive: false,
  },
  {
    info: "place-content is active on flex container",
    property: "place-content",
    tagName: "div",
    rules: ["div { place-content: center;  display: flex;}"],
    isActive: true,
  },
  {
    info: "place-content is active on inline-flex container",
    property: "place-content",
    tagName: "div",
    rules: ["div { place-content: center;  display: inline-flex;}"],
    isActive: true,
  },
  {
    info: "place-content is active on grid container",
    property: "place-content",
    tagName: "div",
    rules: ["div { place-content: center;  display: grid;}"],
    isActive: true,
  },
  {
    info: "place-content is active on inline grid container",
    property: "place-content",
    tagName: "div",
    rules: ["div { place-content: center;  display: inline-grid;}"],
    isActive: true,
  },
];
