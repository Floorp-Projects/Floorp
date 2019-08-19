/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that applicable media queries are updated in the Rule view after reloading
// the page and resizing the window.

const TEST_URI = `
  <style type='text/css'>
    @media all and (max-width: 500px) {
      div {
        color: red;
      }
    }
    @media all and (min-width: 500px) {
      div {
        color: green;
      }
    }
  </style>
  <div></div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {
    inspector,
    view: ruleView,
    testActor,
    toolbox,
  } = await openRuleView();
  const hostWindow = toolbox.win.parent;

  const originalWidth = hostWindow.outerWidth;
  const originalHeight = hostWindow.outerHeight;

  await selectNode("div", inspector);

  info("Resize window so the media query for small viewports applies");
  let onRuleViewRefreshed = ruleView.once("ruleview-refreshed");
  let onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(400, 400);
  await onResize;
  await testActor.reflow();
  await onRuleViewRefreshed;
  let rule = getRuleViewRuleEditor(ruleView, 1).rule;
  is(rule.textProps[0].value, "red", "Small viewport media query inspected");

  info("Reload the current page");
  await reloadPage(inspector, testActor);
  await selectNode("div", inspector);

  info("Resize window so the media query for large viewports applies");
  onRuleViewRefreshed = ruleView.once("ruleview-refreshed");
  onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(800, 800);
  await onResize;
  await testActor.reflow();
  await onRuleViewRefreshed;
  info("Reselect the rule after page reload.");
  rule = getRuleViewRuleEditor(ruleView, 1).rule;
  is(rule.textProps[0].value, "green", "Large viewport media query inspected");

  info("Resize window to original dimentions");
  onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(originalWidth, originalHeight);
  await onResize;
});
