/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that we correctly display appropriate media query titles in the
// property view.

const TEST_URI = TEST_URL_ROOT + "doc_media_queries.html";

let {PropertyView} = devtools.require("devtools/styleinspector/computed-view");
let {CssLogic} = devtools.require("devtools/styleinspector/css-logic");

let test = asyncTest(function*() {
  yield addTab(TEST_URI);
  let {toolbox, inspector, view} = yield openComputedView();

  info("Selecting the test element");
  yield selectNode("div", inspector);

  info("Checking property view");
  yield checkPropertyView(view);
});

function checkPropertyView(view) {
  let propertyView = new PropertyView(view, "width");
  propertyView.buildMain();
  propertyView.buildSelectorContainer();
  propertyView.matchedExpanded = true;

  return propertyView.refreshMatchedSelectors().then(() => {
    let numMatchedSelectors = propertyView.matchedSelectors.length;

    is(numMatchedSelectors, 2,
        "Property view has the correct number of matched selectors for div");

    is(propertyView.hasMatchedSelectors, true,
        "hasMatchedSelectors returns true");
  });
}
