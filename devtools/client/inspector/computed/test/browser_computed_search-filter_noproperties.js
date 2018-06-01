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

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openComputedView();
  const propertyViews = view.propertyViews;

  info("Select the #matches node");
  const matchesNode = await getNodeFront("#matches", inspector);
  let onRefresh = inspector.once("computed-view-refreshed");
  await selectNode(matchesNode, inspector);
  await onRefresh;

  ok(propertyViews.filter(p => p.visible).length > 0, "CSS properties are displayed");
  ok(view.noResults.hasAttribute("hidden"), "no-results message is hidden");

  info("Select a comment node");
  const commentNode = await inspector.walker.previousSibling(matchesNode);
  await selectNode(commentNode, inspector);

  is(propertyViews.filter(p => p.visible).length, 0, "No properties displayed");
  ok(!view.noResults.hasAttribute("hidden"), "no-results message is displayed");

  info("Select the #matches node again");
  onRefresh = inspector.once("computed-view-refreshed");
  await selectNode(matchesNode, inspector);
  await onRefresh;

  ok(propertyViews.filter(p => p.visible).length > 0, "CSS properties are displayed");
  ok(view.noResults.hasAttribute("hidden"), "no-results message is hidden");

  info("Filter by 'will-not-match' and check the no-results message is displayed");
  const searchField = view.searchField;
  searchField.focus();
  synthesizeKeys("will-not-match", view.styleWindow);
  await inspector.once("computed-view-refreshed");

  is(propertyViews.filter(p => p.visible).length, 0, "No properties displayed");
  ok(!view.noResults.hasAttribute("hidden"), "no-results message is displayed");
});
