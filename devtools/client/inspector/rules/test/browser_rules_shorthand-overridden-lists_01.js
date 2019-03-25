/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that overridden longhand properties aren't shown when the shorthand's value
// contains a CSS variable. When this happens, the longhand values can't be computed
// properly and are hidden. So the overridden longhand that are normally auto-expanded
// should be hidden too.

var TEST_URI = `
  <style type="text/css">
    div {
      --color: red;
      background: var(--color);
      background-repeat: no-repeat;
    }
  </style>
  <div>Inspect me</div>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();
  await selectNode("div", inspector);

  const rule = getRuleViewRuleEditor(view, 1).rule;
  const shorthandOverridden = rule.textProps[1].editor.shorthandOverridden;

  is(shorthandOverridden.children.length, 0, "The shorthandOverridden list is empty");
});
