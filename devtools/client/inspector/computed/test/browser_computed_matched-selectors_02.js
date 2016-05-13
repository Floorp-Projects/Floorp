/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests for matched selector texts in the computed view.

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8,<div style='color:blue;'></div>");
  let {inspector, view} = yield openComputedView();
  yield selectNode("div", inspector);

  info("Checking the color property view");
  let propertyView = getPropertyView(view, "color");
  ok(propertyView, "found PropertyView for color");
  is(propertyView.hasMatchedSelectors, true, "hasMatchedSelectors is true");

  info("Expanding the matched selectors");
  propertyView.matchedExpanded = true;
  yield propertyView.refreshMatchedSelectors();

  let span = propertyView.matchedSelectorsContainer
    .querySelector("span.rule-text");
  ok(span, "Found the first table row");

  let selector = propertyView.matchedSelectorViews[0];
  ok(selector, "Found the first matched selector view");
});

function getPropertyView(computedView, name) {
  let propertyView = null;
  computedView.propertyViews.some(function (view) {
    if (view.name == name) {
      propertyView = view;
      return true;
    }
    return false;
  });
  return propertyView;
}
