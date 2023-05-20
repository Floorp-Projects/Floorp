/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `align-content` test cases.

export default [
  {
    info: "align-content is inactive on block elements (until bug 1105571 is fixed)",
    property: "align-content",
    tagName: "div",
    rules: ["div { align-content: center; }"],
    isActive: false,
  },
  {
    info: "align-content is active on flex containers",
    property: "align-content",
    tagName: "div",
    rules: ["div { align-content: center; display: flex; }"],
    isActive: true,
  },
  {
    info: "align-content is active on grid containers",
    property: "align-content",
    tagName: "div",
    rules: ["div { align-content: center; display: grid; }"],
    isActive: true,
  },
  {
    info: "align-content is inactive on flex items",
    property: "align-content",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      const element = document.createElement("span");
      container.append(element);
      rootNode.append(container);
      return element;
    },
    rules: ["div { display: flex; }", "span { align-content: center; }"],
    ruleIndex: 1,
    isActive: false,
  },
  {
    info: "align-content is inactive on grid items",
    property: "align-content",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      const element = document.createElement("span");
      container.append(element);
      rootNode.append(container);
      return element;
    },
    rules: ["div { display: grid; }", "span { align-content: center; }"],
    ruleIndex: 1,
    isActive: false,
  },
  {
    info: "align-content:baseline is active on flex items",
    property: "align-content",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      const element = document.createElement("span");
      container.append(element);
      rootNode.append(container);
      return element;
    },
    rules: ["div { display: flex; }", "span { align-content: baseline; }"],
    ruleIndex: 1,
    isActive: true,
  },
  {
    info: "align-content:baseline is active on grid items",
    property: "align-content",
    createTestElement: rootNode => {
      const container = document.createElement("div");
      const element = document.createElement("span");
      container.append(element);
      rootNode.append(container);
      return element;
    },
    rules: ["div { display: grid; }", "span { align-content: baseline; }"],
    ruleIndex: 1,
    isActive: true,
  },
  {
    info: "align-content:baseline is active on table cells",
    property: "align-content",
    tagName: "div",
    rules: ["div { display: table-cell; align-content: baseline; }"],
    isActive: true,
  },
];
