/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// Tests for preview through Babel's compile output.
requestLongerTimeout(3);

function getOriginalScope(dbg) {
  return dbg.selectors.getSelectedOriginalScope(
    dbg.selectors.getCurrentThread()
  );
}

async function previewToken(dbg, line, column, value) {
  const previewEl = await tryHovering(dbg, line, column, "previewPopup");
  is(previewEl.innerText, value);
  dbg.actions.clearPreview(getContext(dbg));
}

// Test pausing with mapScopes enabled and disabled
add_task(async function() {
  const dbg = await initDebugger("doc-sourcemapped.html");
  dbg.actions.toggleMapScopes();

  info("1. Pause on line 20");
  const filename = "webpack3-babel6://./esmodules-cjs/input.";
  await waitForSources(dbg, filename);
  const source = findSource(dbg, filename);
  await selectSource(dbg, source);
  await addBreakpoint(dbg, source, 20, 2);
  invokeInTab("webpack3Babel6EsmodulesCjs");
  await waitForPaused(dbg);

  info("2. Hover on a token with mapScopes enabled");
  await previewToken(dbg, 20, 16, '"a-default"');
  ok(getOriginalScope(dbg) != null, "Scopes are mapped");

  info("3. Hover on a token with mapScopes disabled");
  clickElement(dbg, "mapScopesCheckbox");
  await previewToken(dbg, 21, 16, "undefined");

  info("4. StepOver with mapScopes disabled");
  await stepOver(dbg);
  await previewToken(dbg, 20, 16, "undefined");
  ok(getOriginalScope(dbg) == null, "Scopes are not mapped");
});
