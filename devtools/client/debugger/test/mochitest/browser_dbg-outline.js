/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests that outline panel can sort functions alphabetically.

"use strict";

add_task(async function () {
  const dbg = await initDebugger("doc-scripts.html", "simple1.js");

  openOutlinePanel(dbg, false);

  is(
    findAllElements(dbg, "outlineItems").length,
    0,
    " There are no outline items when no source is selected"
  );
  is(
    findElementWithSelector(dbg, ".outline-pane-info").innerText,
    "No file selected",
    "The correct message is displayed when there are no outline items"
  );

  const sourcesTab = findElementWithSelector(dbg, ".sources-tab a");
  EventUtils.synthesizeMouseAtCenter(sourcesTab, {}, sourcesTab.ownerGlobal);
  await waitForSourcesInSourceTree(dbg, [], { noExpand: true });

  await selectSource(dbg, "simple1.js", 1);

  await openOutlinePanel(dbg);

  assertOutlineItems(dbg, [
    "λmain()",
    "λdoEval()",
    "λevaledFunc()",
    "λdoNamedEval()",
    // evaledFunc is set twice
    "λevaledFunc()",
    "class MyClass",
    "λconstructor(a, b)",
    "λtest()",
    "λ#privateFunc(a, b)",
    "class Klass",
    "λconstructor()",
    "λtest()",
  ]);

  info("Sort the list");
  findElementWithSelector(dbg, ".outline-footer button").click();
  // Button becomes active to show alphabetization
  is(
    findElementWithSelector(dbg, ".outline-footer button").className,
    "active",
    "Alphabetize button is highlighted when active"
  );

  info("Check that the list was sorted as expected");
  assertOutlineItems(dbg, [
    "λdoEval()",
    "λdoNamedEval()",
    // evaledFunc is set twice
    "λevaledFunc()",
    "λevaledFunc()",
    "λmain()",
    "class Klass",
    "λconstructor()",
    "λtest()",
    "class MyClass",
    "λ#privateFunc(a, b)",
    "λconstructor(a, b)",
    "λtest()",
  ]);
});

// Test empty panel when source has not function or class symbols
add_task(async function () {
  const dbg = await initDebugger("doc-on-load.html", "top-level.js");
  await selectSource(dbg, "top-level.js", 1);

  openOutlinePanel(dbg, false);
  await waitFor(
    () =>
      dbg.win.document.querySelector(".outline-pane-info").innerText ==
      "No functions"
  );

  is(
    findAllElements(dbg, "outlineItems").length,
    0,
    " There are no outline items when no source is selected"
  );
});
