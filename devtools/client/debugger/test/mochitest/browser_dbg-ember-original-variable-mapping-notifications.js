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
      info("Assert the original variable mapping notifications are visible");
      is(
        getScopeNotificationMessage(dbg),
        DEBUGGER_L10N.getFormatStr(
          "scopes.noOriginalScopes",
          DEBUGGER_L10N.getStr("scopes.showOriginalScopes")
        ),
        "Original mapping is disabled so the scopes notification is visible"
      );

      // Open the expressions pane
      let notificationText;
      const notificationVisible = waitUntil(() => {
        notificationText = getExpressionNotificationMessage(dbg);
        return notificationText;
      });
      await toggleExpressions(dbg);
      await notificationVisible;

      is(
        notificationText,
        DEBUGGER_L10N.getStr("expressions.noOriginalScopes"),
        "Original mapping is disabled so the expressions notification is visible"
      );

      await toggleMapScopes(dbg);

      info(
        "Assert the original variable mapping notifications no longer visible"
      );
      ok(
        !getScopeNotificationMessage(dbg),
        "Original mapping is enabled so the scopes notification is no longer visible"
      );
      ok(
        !getScopeNotificationMessage(dbg),
        "Original mapping is enabled so the expressions notification is no longer visible"
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

function getExpressionNotificationMessage(dbg) {
  return dbg.win.document.querySelector(
    ".watch-expressions-pane .pane-info.no-original-scopes-info"
  )?.innerText;
}
