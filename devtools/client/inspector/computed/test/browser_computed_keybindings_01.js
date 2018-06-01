/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests computed view key bindings.

const TEST_URI = `
  <style type="text/css">
    .matches {
      color: #F00;
    }
  </style>
  <span class="matches">Some styled text</span>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const {inspector, view} = await openComputedView();
  await selectNode(".matches", inspector);

  const propView = getFirstVisiblePropertyView(view);
  const rulesTable = propView.matchedSelectorsContainer;
  const matchedExpander = propView.element;

  info("Focusing the property");
  matchedExpander.scrollIntoView();
  const onMatchedExpanderFocus = once(matchedExpander, "focus", true);
  EventUtils.synthesizeMouseAtCenter(matchedExpander, {}, view.styleWindow);
  await onMatchedExpanderFocus;

  await checkToggleKeyBinding(view.styleWindow, "VK_SPACE", rulesTable,
                              inspector);
  await checkToggleKeyBinding(view.styleWindow, "VK_RETURN", rulesTable,
                              inspector);
  await checkHelpLinkKeybinding(view);
});

function getFirstVisiblePropertyView(view) {
  let propView = null;
  view.propertyViews.some(p => {
    if (p.visible) {
      propView = p;
      return true;
    }
    return false;
  });

  return propView;
}

async function checkToggleKeyBinding(win, key, rulesTable, inspector) {
  info("Pressing " + key + " key a couple of times to check that the " +
    "property gets expanded/collapsed");

  const onExpand = inspector.once("computed-view-property-expanded");
  const onCollapse = inspector.once("computed-view-property-collapsed");

  info("Expanding the property");
  EventUtils.synthesizeKey(key, {}, win);
  await onExpand;
  isnot(rulesTable.innerHTML, "", "The property has been expanded");

  info("Collapsing the property");
  EventUtils.synthesizeKey(key, {}, win);
  await onCollapse;
  is(rulesTable.innerHTML, "", "The property has been collapsed");
}

function checkHelpLinkKeybinding(view) {
  info("Check that MDN link is opened on \"F1\"");
  const propView = getFirstVisiblePropertyView(view);
  return new Promise(resolve => {
    propView.mdnLinkClick = function(event) {
      ok(true, "Pressing F1 opened the MDN link");
      resolve();
    };
    EventUtils.synthesizeKey("VK_F1", {}, view.styleWindow);
  });
}
