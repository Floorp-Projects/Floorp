/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

// Tests for preview through Babel's compile output.
requestLongerTimeout(5);

// Test pausing with mapScopes enabled and disabled
add_task(async function () {
  await pushPref("devtools.debugger.map-scopes-enabled", true);
  const dbg = await initDebugger("doc-sourcemapped.html");

  info("1. Pause on line 20");
  const url = "webpack3-babel6://./esmodules-cjs/input.js";
  await waitForSources(dbg, url);

  const source = findSource(dbg, url);
  await selectSource(dbg, source);
  await addBreakpoint(dbg, source, 20, 3);

  invokeInTab("webpack3Babel6EsmodulesCjs");
  await waitForPaused(dbg);

  ok(getOriginalScope(dbg) != null, "Scopes are now mapped");

  ok(!findFooterNotificationMessage(dbg), "No footer notification message");
  await assertPreviewTextValue(dbg, 20, 20, {
    result: '"a-default"',
    expression: "aDefault",
  });

  info("3. Disable original variable mapping");
  await toggleMapScopes(dbg);

  const notificationMessage = DEBUGGER_L10N.getFormatStr(
    "editorNotificationFooter.noOriginalScopes",
    DEBUGGER_L10N.getStr("scopes.showOriginalScopes")
  );

  info(
    "Assert that previews are disabled and the footer notification is visible"
  );
  await hoverAtPos(dbg, { line: 20, column: 17 });
  await assertNoTooltip(dbg);
  is(
    findFooterNotificationMessage(dbg),
    notificationMessage,
    "The Original variable mapping warning is displayed"
  );

  info("4. StepOver with mapScopes disabled");
  await stepOver(dbg, { shouldWaitForLoadedScopes: false });

  info(
    "Assert that previews are still disabled and the footer notification is visible"
  );
  await hoverAtPos(dbg, { line: 20, column: 17 });
  await assertNoTooltip(dbg);

  is(
    findFooterNotificationMessage(dbg),
    notificationMessage,
    "The Original variable mapping warning is displayed"
  );
});

function getOriginalScope(dbg) {
  return dbg.selectors.getSelectedOriginalScope(
    dbg.selectors.getCurrentThread()
  );
}

function findFooterNotificationMessage(dbg) {
  return dbg.win.document.querySelector(".editor-notification-footer")
    ?.innerText;
}
