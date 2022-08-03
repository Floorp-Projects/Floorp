/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests various output of the computed-view's getNodeInfo method.
// This method is used by the HighlightersOverlay and TooltipsOverlay on mouseover to
// decide which highlighter or tooltip to show when hovering over a value/name/selector
// if any.
//
// For instance, browser_ruleview_selector-highlighter_01.js and
// browser_ruleview_selector-highlighter_02.js test that the selector
// highlighter appear when hovering over a selector in the rule-view.
// Since the code to make this work for the computed-view is 90% the same,
// there is no need for testing it again here.
// This test however serves as a unit test for getNodeInfo.

const {
  VIEW_NODE_SELECTOR_TYPE,
  VIEW_NODE_PROPERTY_TYPE,
  VIEW_NODE_VALUE_TYPE,
  VIEW_NODE_IMAGE_URL_TYPE,
} = require("devtools/client/inspector/shared/node-types");

const TEST_URI = `
  <style type="text/css">
    body {
      background: red;
      color: white;
    }
    div {
      background: green;
    }
    div div {
      background-color: yellow;
      background-image: url(chrome://branding/content/icon64.png);
      color: red;
    }
  </style>
  <div><div id="testElement">Test element</div></div>
`;

// Each item in this array must have the following properties:
// - desc {String} will be logged for information
// - getHoveredNode {Generator Function} received the computed-view instance as
//   argument and must return the node to be tested
// - assertNodeInfo {Function} should check the validity of the nodeInfo
//   argument it receives
const TEST_DATA = [
  {
    desc: "Testing a null node",
    getHoveredNode() {
      return null;
    },
    assertNodeInfo(nodeInfo) {
      is(nodeInfo, null);
    },
  },
  {
    desc: "Testing a useless node",
    getHoveredNode(view) {
      return view.element;
    },
    assertNodeInfo(nodeInfo) {
      is(nodeInfo, null);
    },
  },
  {
    desc: "Testing a property name",
    getHoveredNode(view) {
      return getComputedViewProperty(view, "color").nameSpan;
    },
    assertNodeInfo(nodeInfo) {
      is(nodeInfo.type, VIEW_NODE_PROPERTY_TYPE);
      ok("property" in nodeInfo.value);
      ok("value" in nodeInfo.value);
      is(nodeInfo.value.property, "color");
      is(nodeInfo.value.value, "rgb(255, 0, 0)");
    },
  },
  {
    desc: "Testing a property value",
    getHoveredNode(view) {
      return getComputedViewProperty(view, "color").valueSpan;
    },
    assertNodeInfo(nodeInfo) {
      is(nodeInfo.type, VIEW_NODE_VALUE_TYPE);
      ok("property" in nodeInfo.value);
      ok("value" in nodeInfo.value);
      is(nodeInfo.value.property, "color");
      is(nodeInfo.value.value, "rgb(255, 0, 0)");
    },
  },
  {
    desc: "Testing an image url",
    getHoveredNode(view) {
      const { valueSpan } = getComputedViewProperty(view, "background-image");
      return valueSpan.querySelector(".theme-link");
    },
    assertNodeInfo(nodeInfo) {
      is(nodeInfo.type, VIEW_NODE_IMAGE_URL_TYPE);
      ok("property" in nodeInfo.value);
      ok("value" in nodeInfo.value);
      is(nodeInfo.value.property, "background-image");
      is(nodeInfo.value.value, 'url("chrome://branding/content/icon64.png")');
      is(nodeInfo.value.url, "chrome://branding/content/icon64.png");
    },
  },
  {
    desc: "Testing a matched rule selector (bestmatch)",
    async getHoveredNode(view) {
      const el = await getComputedViewMatchedRules(view, "background-color");
      return el.querySelector(".bestmatch");
    },
    assertNodeInfo(nodeInfo) {
      is(nodeInfo.type, VIEW_NODE_SELECTOR_TYPE);
      is(nodeInfo.value, "div div");
    },
  },
  {
    desc: "Testing a matched rule selector (matched)",
    async getHoveredNode(view) {
      const el = await getComputedViewMatchedRules(view, "background-color");
      return el.querySelector(".matched");
    },
    assertNodeInfo(nodeInfo) {
      is(nodeInfo.type, VIEW_NODE_SELECTOR_TYPE);
      is(nodeInfo.value, "div");
    },
  },
  {
    desc: "Testing a matched rule selector (parentmatch)",
    async getHoveredNode(view) {
      const el = await getComputedViewMatchedRules(view, "color");
      return el.querySelector(".parentmatch");
    },
    assertNodeInfo(nodeInfo) {
      is(nodeInfo.type, VIEW_NODE_SELECTOR_TYPE);
      is(nodeInfo.value, "body");
    },
  },
  {
    desc: "Testing a matched rule value",
    async getHoveredNode(view) {
      const el = await getComputedViewMatchedRules(view, "color");
      return el.querySelector(".computed-other-property-value");
    },
    assertNodeInfo(nodeInfo) {
      is(nodeInfo.type, VIEW_NODE_VALUE_TYPE);
      is(nodeInfo.value.property, "color");
      is(nodeInfo.value.value, "red");
    },
  },
  {
    desc: "Testing a matched rule stylesheet link",
    async getHoveredNode(view) {
      const el = await getComputedViewMatchedRules(view, "color");
      return el.querySelector(".rule-link .theme-link");
    },
    assertNodeInfo(nodeInfo) {
      is(nodeInfo, null);
    },
  },
];

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openComputedView();
  await selectNode("#testElement", inspector);

  for (const { desc, getHoveredNode, assertNodeInfo } of TEST_DATA) {
    info(desc);
    const nodeInfo = view.getNodeInfo(await getHoveredNode(view));
    assertNodeInfo(nodeInfo);
  }
});
