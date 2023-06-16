/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test for variables in rule view.

const TEST_URI = URL_ROOT + "doc_variables_4.html";

add_task(async function () {
  await addTab(TEST_URI);
  const { inspector, view } = await openRuleView();

  await testNumber(inspector, view);
  await testDash(inspector, view);
});

async function testNumber(inspector, view) {
  info(
    "Test support for allowing vars that begin with a number" +
      "Format: --10: 10px;"
  );

  await selectNode("#a", inspector);

  const upperCaseVarEl = getRuleViewProperty(
    view,
    "#a",
    "font-size"
  ).valueSpan.querySelector(".ruleview-variable");

  is(
    upperCaseVarEl.dataset.variable,
    "--10 = 10px",
    "variable that starts with a number is valid"
  );
}

async function testDash(inspector, view) {
  info(
    "Test support for allowing vars that begin with a dash" +
      "Format: ---blue: blue;"
  );

  await selectNode("#b", inspector);

  const upperCaseVarEl = getRuleViewProperty(
    view,
    "#b",
    "color"
  ).valueSpan.querySelector(".ruleview-variable");

  is(
    upperCaseVarEl.dataset.variable,
    "---blue = blue",
    "variable that starts with a dash is valid"
  );
}
