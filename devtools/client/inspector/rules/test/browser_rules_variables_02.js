/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for variables in rule view.

const TEST_URI = URL_ROOT + "doc_variables_2.html";

add_task(function* () {
  yield addTab(TEST_URI);
  let {inspector, view} = yield openRuleView();

  yield testBasic(inspector, view);
  yield testNestedCssFunctions(inspector, view);
  yield testBorderShorthandAndInheritance(inspector, view);
  yield testSingleLevelVariable(inspector, view);
  yield testDoubleLevelVariable(inspector, view);
  yield testTripleLevelVariable(inspector, view);
});

function* testBasic(inspector, view) {
  info("Test support for basic variable functionality for var() with 2 variables." +
       "Format: var(--var1, var(--var2))");

  yield selectNode("#a", inspector);
  let unsetVar = getRuleViewProperty(view, "#a", "font-size").valueSpan
    .querySelector(".ruleview-unmatched-variable");
  let setVarParent = unsetVar.nextElementSibling;
  let setVar = getVarFromParent(setVarParent);
  is(unsetVar.textContent, "--var-not-defined",
    "--var-not-defined is not set correctly");
  is(unsetVar.dataset.variable, "--var-not-defined is not set",
    "--var-not-defined's dataset.variable is not set correctly");
  is(setVarParent.textContent, " var(--var-defined-font-size)",
    "var(--var-defined-font-size) parsed incorrectly");
  is(setVar.textContent, "--var-defined-font-size",
    "--var-defined-font-size is not set correctly");
  is(setVar.dataset.variable, "--var-defined-font-size = 60px",
    "--bg's dataset.variable is not set correctly");
}

function* testNestedCssFunctions(inspector, view) {
  info("Test support for variable functionality for a var() nested inside " +
  "another CSS function. Format: rgb(0, 0, var(--var1, var(--var2)))");

  yield selectNode("#b", inspector);
  let unsetVarParent = getRuleViewProperty(view, "#b", "color").valueSpan
    .querySelector(".ruleview-unmatched-variable");
  let unsetVar = getVarFromParent(unsetVarParent);
  let setVar = unsetVarParent.previousElementSibling;
  is(unsetVarParent.textContent, " var(--var-defined-r-2)",
    "var(--var-defined-r-2) not parsed correctly");
  is(unsetVar.textContent, "--var-defined-r-2",
    "--var-defined-r-2 is not set correctly");
  is(unsetVar.dataset.variable, "--var-defined-r-2 = 0",
    "--var-defined-r-2's dataset.variable is not set correctly");
  is(setVar.textContent, "--var-defined-r-1",
    "--var-defined-r-1 is not set correctly");
  is(setVar.dataset.variable, "--var-defined-r-1 = 255",
    "--var-defined-r-1's dataset.variable is not set correctly");
}

function* testBorderShorthandAndInheritance(inspector, view) {
  info("Test support for variable functionality for shorthands/CSS styles with spaces " +
  "like \"margin: w x y z\". Also tests functionality for inherticance of CSS" +
  " variables. Format: var(l, var(m)) var(x) rgb(var(r) var(g) var(b))");

  yield selectNode("#c", inspector);
  let unsetVarL = getRuleViewProperty(view, "#c", "border").valueSpan
    .querySelector(".ruleview-unmatched-variable");
  let setVarMParent = unsetVarL.nextElementSibling;

  // var(x) is the next sibling of the parent of M
  let setVarXParent = setVarMParent.parentNode.nextElementSibling;

  // var(r) is the next sibling of var(x), and var(g) is the next sibling of var(r), etc.
  let setVarRParent = setVarXParent.nextElementSibling;
  let setVarGParent = setVarRParent.nextElementSibling;
  let setVarBParent = setVarGParent.nextElementSibling;

  let setVarM = getVarFromParent(setVarMParent);
  let setVarX = setVarXParent.firstElementChild;
  let setVarR = setVarRParent.firstElementChild;
  let setVarG = setVarGParent.firstElementChild;
  let setVarB = setVarBParent.firstElementChild;

  is(unsetVarL.textContent, "--var-undefined",
    "--var-undefined is not set correctly");
  is(unsetVarL.dataset.variable, "--var-undefined is not set",
    "--var-undefined's dataset.variable is not set correctly");

  is(setVarM.textContent, "--var-border-px",
    "--var-border-px is not set correctly");
  is(setVarM.dataset.variable, "--var-border-px = 10px",
    "--var-border-px's dataset.variable is not set correctly");

  is(setVarX.textContent, "--var-border-style",
    "--var-border-style is not set correctly");
  is(setVarX.dataset.variable, "--var-border-style = solid",
    "var-border-style's dataset.variable is not set correctly");

  is(setVarR.textContent, "--var-border-r",
    "--var-defined-r is not set correctly");
  is(setVarR.dataset.variable, "--var-border-r = 255",
    "--var-defined-r's dataset.variable is not set correctly");

  is(setVarG.textContent, "--var-border-g",
    "--var-defined-g is not set correctly");
  is(setVarG.dataset.variable, "--var-border-g = 0",
    "--var-defined-g's dataset.variable is not set correctly");

  is(setVarB.textContent, "--var-border-b",
    "--var-defined-b is not set correctly");
  is(setVarB.dataset.variable, "--var-border-b = 0",
    "--var-defined-b's dataset.variable is not set correctly");
}

function* testSingleLevelVariable(inspector, view) {
  info("Test support for variable functionality of a single level of " +
  "undefined variables. Format: var(x, constant)");

  yield selectNode("#d", inspector);
  let unsetVar = getRuleViewProperty(view, "#d", "font-size").valueSpan
    .querySelector(".ruleview-unmatched-variable");

  is(unsetVar.textContent, "--var-undefined",
    "--var-undefined is not set correctly");
  is(unsetVar.dataset.variable, "--var-undefined is not set",
    "--var-undefined's dataset.variable is not set correctly");
}

function* testDoubleLevelVariable(inspector, view) {
  info("Test support for variable functionality of double level of " +
  "undefined variables. Format: var(x, var(y, constant))");

  yield selectNode("#e", inspector);
  let allUnsetVars = getRuleViewProperty(view, "#e", "color").valueSpan
    .querySelectorAll(".ruleview-unmatched-variable");

  is(allUnsetVars.length, 2, "The number of unset variables is mismatched.");

  let unsetVar1 = allUnsetVars[0];
  let unsetVar2 = allUnsetVars[1];

  is(unsetVar1.textContent, "--var-undefined",
    "--var-undefined is not set correctly");
  is(unsetVar1.dataset.variable, "--var-undefined is not set",
    "--var-undefined's dataset.variable is not set correctly");

  is(unsetVar2.textContent, "--var-undefined-2",
    "--var-undefined is not set correctly");
  is(unsetVar2.dataset.variable, "--var-undefined-2 is not set",
    "--var-undefined-2's dataset.variable is not set correctly");
}

function* testTripleLevelVariable(inspector, view) {
  info("Test support for variable functionality of triple level of " +
  "undefined variables. Format: var(x, var(y, var(z, constant)))");

  yield selectNode("#f", inspector);
  let allUnsetVars = getRuleViewProperty(view, "#f", "border-style").valueSpan
    .querySelectorAll(".ruleview-unmatched-variable");

  is(allUnsetVars.length, 3, "The number of unset variables is mismatched.");

  let unsetVar1 = allUnsetVars[0];
  let unsetVar2 = allUnsetVars[1];
  let unsetVar3 = allUnsetVars[2];

  is(unsetVar1.textContent, "--var-undefined",
    "--var-undefined is not set correctly");
  is(unsetVar1.dataset.variable, "--var-undefined is not set",
    "--var-undefined's dataset.variable is not set correctly");

  is(unsetVar2.textContent, "--var-undefined-2",
    "--var-undefined-2 is not set correctly");
  is(unsetVar2.dataset.variable, "--var-undefined-2 is not set",
    "--var-defined-r-2's dataset.variable is not set correctly");

  is(unsetVar3.textContent, "--var-undefined-3",
    "--var-undefined-3 is not set correctly");
  is(unsetVar3.dataset.variable, "--var-undefined-3 is not set",
    "--var-defined-r-3's dataset.variable is not set correctly");
}

function getVarFromParent(varParent) {
  return varParent.firstElementChild.firstElementChild;
}
