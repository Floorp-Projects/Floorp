/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

add_task(async function () {
  await pushPref("devtools.debugger.map-scopes-enabled", true);
  const dbg = await initDebugger("doc-react.html", "App.js");

  await selectSource(dbg, "App.js");
  await addBreakpoint(dbg, "App.js", 11);

  info("Test previewing an immutable Map inside of a react component");
  invokeInTab("clickButton");
  await waitForPaused(dbg);

  await waitForState(dbg, state =>
    dbg.selectors.getSelectedScopeMappings(dbg.selectors.getCurrentThread())
  );

  await assertPreviews(dbg, [
    {
      line: 10,
      column: 22,
      expression: "fields",
      fields: [["size", "1"]],
    },
  ]);

  info("Verify that the file is flagged as a React module");
  const sourceTab = findElementWithSelector(dbg, ".source-tab.active");
  ok(
    sourceTab.querySelector(".source-icon.react"),
    "Source tab has a React icon"
  );
  assertSourceIcon(dbg, "App.js", "react");
});
