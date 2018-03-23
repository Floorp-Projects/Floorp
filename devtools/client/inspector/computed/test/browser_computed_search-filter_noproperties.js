/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the "no-results" message is displayed when selecting an invalid element or
// when all properties have been filtered out.

const TEST_URI = `
  <style type="text/css">
    .matches {
      color: #F00;
      background-color: #00F;
      border-color: #0F0;
    }
  </style>
  <div>
    <!-- comment node -->
    <span id="matches" class="matches">Some styled text</span>
  </div>
`;

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let {inspector, view} = yield openComputedView();
  let propertyViews = view.propertyViews;

  info("Select the #matches node");
  let matchesNode = yield getNodeFront("#matches", inspector);
  let onRefresh = inspector.once("computed-view-refreshed");
  yield selectNode(matchesNode, inspector);
  yield onRefresh;

  ok(propertyViews.filter(p => p.visible).length > 0, "CSS properties are displayed");
  ok(view.noResults.hasAttribute("hidden"), "no-results message is hidden");

  info("Select a comment node");
  let commentNode = yield inspector.walker.previousSibling(matchesNode);
  yield selectNode(commentNode, inspector);

  is(propertyViews.filter(p => p.visible).length, 0, "No properties displayed");
  ok(!view.noResults.hasAttribute("hidden"), "no-results message is displayed");

  info("Select the #matches node again");
  onRefresh = inspector.once("computed-view-refreshed");
  yield selectNode(matchesNode, inspector);
  yield onRefresh;

  ok(propertyViews.filter(p => p.visible).length > 0, "CSS properties are displayed");
  ok(view.noResults.hasAttribute("hidden"), "no-results message is hidden");

  info("Filter by 'will-not-match' and check the no-results message is displayed");
  let searchField = view.searchField;
  searchField.focus();
  synthesizeKeys("will-not-match", view.styleWindow);
  yield inspector.once("computed-view-refreshed");

  is(propertyViews.filter(p => p.visible).length, 0, "No properties displayed");
  ok(!view.noResults.hasAttribute("hidden"), "no-results message is displayed");
});
