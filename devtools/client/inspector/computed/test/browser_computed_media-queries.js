/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that we correctly display appropriate media query titles in the
// property view.

const TEST_URI = TEST_URL_ROOT + "doc_media_queries.html";

var {PropertyView} = require("devtools/client/inspector/computed/computed");
var {CssLogic} = require("devtools/shared/inspector/css-logic");

add_task(function*() {
  yield addTab(TEST_URI);
  let {inspector, view} = yield openComputedView();
  yield selectNode("div", inspector);
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
