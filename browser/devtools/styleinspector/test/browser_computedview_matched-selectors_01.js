/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Checking selector counts, matched rules and titles in the computed-view

const {PropertyView} = devtools.require("devtools/styleinspector/computed-view");
const TEST_URI = TEST_URL_ROOT + "doc_matched_selectors.html";

let test = asyncTest(function*() {
  yield addTab(TEST_URI);
  let {toolbox, inspector, view} = yield openComputedView();

  yield selectNode("#test", inspector);
  yield testMatchedSelectors(view, inspector);
});

function* testMatchedSelectors(view, inspector) {
  info("checking selector counts, matched rules and titles");

  let nodeFront = yield getNodeFront("#test", inspector);
  is(nodeFront, view.viewedElement, "style inspector node matches the selected node");

  let propertyView = new PropertyView(view, "color");
  propertyView.buildMain();
  propertyView.buildSelectorContainer();
  propertyView.matchedExpanded = true;

  yield propertyView.refreshMatchedSelectors();

  let numMatchedSelectors = propertyView.matchedSelectors.length;
  is(numMatchedSelectors, 6, "CssLogic returns the correct number of matched selectors for div");
  is(propertyView.hasMatchedSelectors, true, "hasMatchedSelectors returns true");
}
