/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

add_task(async function() {
  const dbg = await initDebugger("doc-react.html", "App.js");

  await selectSource(dbg, "App.js");
  await addBreakpoint(dbg, "App.js", 11);

  info("Test previewing an immutable Map inside of a react component");
  invokeInTab("clickButton");
  await waitForPaused(dbg);

  const {
    selectors: { getSelectedScopeMappings, getCurrentThread }
  } = dbg;

  await waitForState(dbg, state =>
    getSelectedScopeMappings(getCurrentThread())
  );

  await assertPreviewTextValue(dbg, 10, 22, {
    text: "size: 1",
    expression: "_this.fields;"
  });
});
