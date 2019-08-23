/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a CSS property is marked as inactive when a condition
// changes in other CSS rule matching the element.

const TEST_URI = `
<style>
  .display {
    display: grid;
  }
  .gap {
    gap: 1em;
  }
</style>
<div class="display gap">`;

add_task(async function() {
  await pushPref("devtools.inspector.inactive.css.enabled", true);
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  await selectNode("div", inspector);

  checkDeclarationIsActive(view, 1, { gap: "1em" });
  await toggleDeclaration(inspector, view, 2, { display: "grid" });
  checkDeclarationIsInactive(view, 1, { gap: "1em" });
});
