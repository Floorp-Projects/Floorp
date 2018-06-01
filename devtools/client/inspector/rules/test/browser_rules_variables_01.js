/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for variables in rule view.

const TEST_URI = URL_ROOT + "doc_variables_1.html";

add_task(async function() {
  await addTab(TEST_URI);
  const {inspector, view} = await openRuleView();
  await selectNode("#target", inspector);

  info("Tests basic support for CSS Variables for both single variable " +
  "and double variable. Formats tested: var(x, constant), var(x, var(y))");

  const unsetColor = getRuleViewProperty(view, "div", "color").valueSpan
    .querySelector(".ruleview-unmatched-variable");
  const setColor = unsetColor.previousElementSibling;
  is(unsetColor.textContent, " red", "red is unmatched in color");
  is(setColor.textContent, "--color", "--color is not set correctly");
  is(setColor.dataset.variable, "--color = chartreuse",
                                "--color's dataset.variable is not set correctly");
  let previewTooltip = await assertShowPreviewTooltip(view, setColor);
  await assertTooltipHiddenOnMouseOut(previewTooltip, setColor);

  const unsetVar = getRuleViewProperty(view, "div", "background-color").valueSpan
    .querySelector(".ruleview-unmatched-variable");
  const setVar = unsetVar.nextElementSibling;
  const setVarName = setVar.firstElementChild.firstElementChild;
  is(unsetVar.textContent, "--not-set",
     "--not-set is unmatched in background-color");
  is(setVar.textContent, " var(--bg)", "var(--bg) parsed incorrectly");
  is(setVarName.textContent, "--bg", "--bg is not set correctly");
  is(setVarName.dataset.variable, "--bg = seagreen",
                                  "--bg's dataset.variable is not set correctly");
  previewTooltip = await assertShowPreviewTooltip(view, setVarName);
  await assertTooltipHiddenOnMouseOut(previewTooltip, setVarName);
});
