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

ignoreGetGridsPromiseRejections();

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view: ruleView, toolbox } = await openRuleView();
  const hostWindow = toolbox.win.parent;

  const originalWidth = hostWindow.outerWidth;
  const originalHeight = hostWindow.outerHeight;

  await selectNode("div", inspector);

  info("Resize window so the media query for small viewports applies");
  hostWindow.resizeTo(400, 400);

  await waitForMediaRuleColor(ruleView, "red");
  ok(true, "Small viewport media query inspected");

  info("Reload the current page");
  await refreshTab();
  await selectNode("div", inspector);

  info("Resize window so the media query for large viewports applies");
  hostWindow.resizeTo(800, 800);

  info("Reselect the rule after page reload.");
  await waitForMediaRuleColor(ruleView, "green");
  ok(true, "Large viewport media query inspected");

  info("Resize window to original dimentions");
  const onResize = once(hostWindow, "resize");
  hostWindow.resizeTo(originalWidth, originalHeight);
  await onResize;
});

function waitForMediaRuleColor(ruleView, color) {
  return waitUntil(() => {
    try {
      const { value } = getTextProperty(ruleView, 1, { color });
      return value === color;
    } catch (e) {
      return false;
    }
  });
}
