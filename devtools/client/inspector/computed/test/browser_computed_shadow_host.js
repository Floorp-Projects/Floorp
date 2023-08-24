/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  PropertyView,
} = require("resource://devtools/client/inspector/computed/computed.js");

// Test matched selectors for a :host selector in the computed view.

const SHADOW_DOM = `<style>
  :host {
    color: red;
  }

  .test-span {
    color: blue;
  }
</style>
<span class="test-span">test</span>`;

const TEST_PAGE = `
  <div id="host"></div>
  <script>
    const div = document.querySelector("div");
    div.attachShadow({ mode: "open" }).innerHTML = \`${SHADOW_DOM}\`;
  </script>`;

const TEST_URI = `https://example.com/document-builder.sjs?html=${encodeURIComponent(
  TEST_PAGE
)}`;

add_task(async function () {
  await addTab(TEST_URI);
  const { inspector, view } = await openComputedView();

  {
    await selectNode("#host", inspector);
    const propertyView = await getPropertyViewWithSelectors(view, "color");
    const selectors = propertyView.matchedSelectors.map(s => s.selector);
    Assert.deepEqual(
      selectors,
      [":host", ":root"],
      "host has the expected selectors for color"
    );
  }

  {
    const nodeFront = await getNodeFrontInShadowDom(
      ".test-span",
      "#host",
      inspector
    );
    await selectNode(nodeFront, inspector);
    const propertyView = await getPropertyViewWithSelectors(view, "color");
    const selectors = propertyView.matchedSelectors.map(s => s.selector);
    Assert.deepEqual(
      selectors,
      [".test-span", ":host", ":root"],
      "shadow host child has the expected selectors for color"
    );
  }
});

async function getPropertyViewWithSelectors(view, property) {
  const propertyView = new PropertyView(view, property);
  propertyView.createListItemElement();
  propertyView.matchedExpanded = true;

  await propertyView.refreshMatchedSelectors();

  return propertyView;
}
