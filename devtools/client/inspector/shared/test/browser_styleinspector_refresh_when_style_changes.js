/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the rule and computed views refresh when style changes that impact the
// current selection occur.
// This test does not need to worry about the correctness of the styles and rules
// displayed in these views (other tests do this) but only cares that they do catch the
// change.

const TEST_URI = TEST_URL_ROOT + "doc_content_style_changes.html";

const TEST_DATA = [{
  target: "#test",
  className: "green-class",
  force: true
}, {
  target: "#test",
  className: "green-class",
  force: false
}, {
  target: "#parent",
  className: "purple-class",
  force: true
}, {
  target: "#parent",
  className: "purple-class",
  force: false
}, {
  target: "#sibling",
  className: "blue-class",
  force: true
}, {
  target: "#sibling",
  className: "blue-class",
  force: false
}];

add_task(async function() {
  const tab = await addTab(TEST_URI);

  const { inspector } = await openRuleView();
  await selectNode("#test", inspector);

  info("Run the test on the rule-view");
  await runViewTest(inspector, tab, "rule");

  info("Switch to the computed view");
  const onComputedViewReady = inspector.once("computed-view-refreshed");
  selectComputedView(inspector);
  await onComputedViewReady;

  info("Run the test again on the computed view");
  await runViewTest(inspector, tab, "computed");
});

async function runViewTest(inspector, tab, viewName) {
  for (const { target, className, force } of TEST_DATA) {
    info((force ? "Adding" : "Removing") +
         ` class ${className} on ${target} and expecting a ${viewName}-view refresh`);

    await toggleClassAndWaitForViewChange(
      { target, className, force }, inspector, tab, `${viewName}-view-refreshed`);
  }
}

async function toggleClassAndWaitForViewChange(whatToMutate, inspector, tab, eventName) {
  const onRefreshed = inspector.once(eventName);

  await ContentTask.spawn(tab.linkedBrowser, whatToMutate,
    function({ target, className, force }) {
      content.document.querySelector(target).classList.toggle(className, force);
    }
  );

  await onRefreshed;
  ok(true, "The view was refreshed after the class was changed");
}
