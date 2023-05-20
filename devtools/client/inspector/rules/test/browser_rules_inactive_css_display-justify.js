/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that a declaration's inactive state doesn't linger on its previous state when
// the declaration it depends on changes. Bug 1593944

const TEST_URI = `
<style>
  div {
    justify-content: center;
    /*! display: flex */
  }
</style>
<div>`;

add_task(async function () {
  await pushPref("devtools.inspector.inactive.css.enabled", true);
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, view } = await openRuleView();

  await selectNode("div", inspector);

  const justifyContent = { "justify-content": "center" };
  const justifyItems = { "justify-items": "center" };
  const displayFlex = { display: "flex" };
  const displayGrid = { display: "grid" };

  info("Enable display:flex and check that justify-content becomes active");
  await checkDeclarationIsInactive(view, 1, justifyContent);
  await toggleDeclaration(view, 1, displayFlex);
  await checkDeclarationIsActive(view, 1, justifyContent);

  info(
    "Rename justify-content to justify-items and check that it becomes inactive"
  );
  await updateDeclaration(view, 1, justifyContent, justifyItems);
  await checkDeclarationIsInactive(view, 1, justifyItems);

  info(
    "Rename display:flex to display:grid and check that justify-items becomes active"
  );
  await updateDeclaration(view, 1, displayFlex, displayGrid);
  await checkDeclarationIsActive(view, 1, justifyItems);
});
