/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule view refreshes when a stylesheet is added or modified

const TEST_URI = "<h1>Hello DevTools</h1>";

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  await selectNode("h1", inspector);

  info("Add a stylesheet with matching rule for the h1 node");
  let onUpdated = inspector.once("rule-view-refreshed");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const addedStylesheet = content.document.createElement("style");
    addedStylesheet.textContent = "h1 { background: tomato }";
    content.document.head.append(addedStylesheet);
  });
  await onUpdated;
  ok(true, "Rules view was refreshed when adding a stylesheet");
  checkRulesViewSelectors(view, ["element", "h1"]);
  is(
    getRuleViewPropertyValue(view, "h1", "background"),
    "tomato",
    "Expected value is displayed for the background property"
  );

  info("Modify the stylesheet added previously");
  onUpdated = inspector.once("rule-view-refreshed");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const addedStylesheet = content.document.querySelector("style");
    addedStylesheet.textContent = "body h1 { background: gold; color: navy; }";
  });
  await onUpdated;
  ok(true, "Rules view was refreshed when updating the stylesheet");
  checkRulesViewSelectors(view, ["element", "body h1"]);
  is(
    getRuleViewPropertyValue(view, "body h1", "background"),
    "gold",
    "Expected value is displayed for the background property"
  );
  is(
    getRuleViewPropertyValue(view, "body h1", "color"),
    "navy",
    "Expected value is displayed for the color property"
  );
});

function checkRulesViewSelectors(view, expectedSelectors) {
  Assert.deepEqual(
    getRuleSelectors(view),
    expectedSelectors,
    "Expected selectors are displayed"
  );
}

function getRuleSelectors(view) {
  return Array.from(
    view.styleDocument.querySelectorAll(".ruleview-selectorcontainer")
  ).map(el => el.textContent);
}
