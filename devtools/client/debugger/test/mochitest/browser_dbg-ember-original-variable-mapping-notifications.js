/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";
// Tests pausing in original sources from projects built on ember framework,
// This also tests the original variable mapping toggle and notifications

add_task(async function () {
  const dbg = await initDebugger("ember/quickstart/dist/");

  await invokeWithBreakpoint(
    dbg,
    "mapTestFunction",
    "router.js",
    { line: 13, column: 3 },
    async () => {
      is(
        getScopeNotificationMessage(dbg),
        DEBUGGER_L10N.getFormatStr(
          "scopes.noOriginalScopes",
          DEBUGGER_L10N.getStr("scopes.showOriginalScopes")
        ),
        "Original mapping is disabled so the notification is visible"
      );
      await toggleMapScopes(dbg);
      ok(
        !getScopeNotificationMessage(dbg),
        "Original mapping is enabled so the notification is no longer visible"
      );
      await assertScopes(dbg, [
        "Module",
        ["config", "{\u2026}"],
        "EmberRouter:Class()",
        "Router:Class()",
      ]);
    },
    { shouldWaitForLoadedScopes: false }
  );
});

function getScopeNotificationMessage(dbg) {
  return dbg.win.document.querySelector(
    ".scopes-pane .pane-info.no-original-scopes-info"
  )?.innerText;
}
