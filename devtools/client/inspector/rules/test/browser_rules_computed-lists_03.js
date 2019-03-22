/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the rule view does not show expanders for property values that contain
// variables.
// This should, in theory, be able to work, but the complexity outlined in
// https://bugzilla.mozilla.org/show_bug.cgi?id=1535315#c2 made us hide the expander
// instead. Also, this is what Chrome does too.

var TEST_URI = `
  <style type="text/css">
    #testid {
      --primary-color: black;
      background: var(--primary-color, red);
    }
  </style>
  <h1 id="testid">Styled Node</h1>
`;

add_task(async function() {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));

  const { inspector, view } = await openRuleView();
  await selectNode("#testid", inspector);
  const rule = getRuleViewRuleEditor(view, 1).rule;

  is(rule.textProps[0].name, "--primary-color", "The first property is the variable");
  is(rule.textProps[1].name, "background", "The second property is background");

  info("Check that the expander is hidden for the background property");
  is(rule.textProps[1].editor.expander.style.display, "none", "Expander is hidden");
});
