/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the computed view is initialized when the computed view is the default tab
// for the inspector.

const TEST_URI = `
  <style type="text/css">
    #matches {
      color: #F00;
    }
  </style>
  <span id="matches">Some styled text</span>
`;

add_task(async function() {
  await pushPref("devtools.inspector.activeSidebar", "computedview");
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openComputedView();
  await selectNode("#matches", inspector);
  is(
    isPropertyVisible("color", view),
    true,
    "span #matches color property is visible"
  );
});

function isPropertyVisible(name, view) {
  info("Checking property visibility for " + name);
  const propertyViews = view.propertyViews;
  for (const propView of propertyViews) {
    if (propView.name == name) {
      return propView.visible;
    }
  }
  return false;
}
