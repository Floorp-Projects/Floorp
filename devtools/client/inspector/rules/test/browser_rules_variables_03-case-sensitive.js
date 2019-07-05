/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that inherited CSS variables are case senstive.

const TEST_URI = URL_ROOT + "doc_variables_3.html";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();
  await selectNode("#target", inspector);

  const upperCaseVarEl = getRuleViewProperty(
    view,
    "div",
    "color"
  ).valueSpan.querySelector(".ruleview-variable");
  const lowerCaseVarEl = getRuleViewProperty(
    view,
    "div",
    "background"
  ).valueSpan.querySelector(".ruleview-variable");

  is(upperCaseVarEl.textContent, "--COLOR", "upper case variable is matched");
  is(
    lowerCaseVarEl.textContent,
    "--background",
    "lower case variable is matched"
  );
});
