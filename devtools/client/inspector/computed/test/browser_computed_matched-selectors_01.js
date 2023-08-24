/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Checking selector counts, matched rules and titles in the computed-view.

const {
  PropertyView,
} = require("resource://devtools/client/inspector/computed/computed.js");
const TEST_URI = URL_ROOT + "doc_matched_selectors.html";

add_task(async function () {
  await addTab(TEST_URI);
  const { inspector, view } = await openComputedView();

  await selectNode("#test", inspector);
  await testMatchedSelectors(view, inspector);
});

async function testMatchedSelectors(view, inspector) {
  info("checking selector counts, matched rules and titles");

  const nodeFront = await getNodeFront("#test", inspector);
  is(
    nodeFront,
    view._viewedElement,
    "style inspector node matches the selected node"
  );

  const propertyView = new PropertyView(view, "color");
  propertyView.createListItemElement();
  propertyView.matchedExpanded = true;

  await propertyView.refreshMatchedSelectors();

  const numMatchedSelectors = propertyView.matchedSelectors.length;
  is(
    numMatchedSelectors,
    7,
    "CssLogic returns the correct number of matched selectors for div"
  );
  is(
    propertyView.hasMatchedSelectors,
    true,
    "hasMatchedSelectors returns true"
  );
}
