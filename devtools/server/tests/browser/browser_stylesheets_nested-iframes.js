/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that StyleSheetsActor.getStyleSheets() works if an iframe does not have
// a content document.

const {StyleSheetsFront} = require("devtools/shared/fronts/stylesheets");

add_task(async function() {
  await addTab(MAIN_DOMAIN + "stylesheets-nested-iframes.html");

  info("Initialising the debugger server and client.");
  initDebuggerServer();
  const client = new DebuggerClient(DebuggerServer.connectPipe());
  const form = await connectDebuggerClient(client);

  info("Attaching to the active tab.");
  await client.attachTab(form.actor);

  const front = StyleSheetsFront(client, form);
  ok(front, "The StyleSheetsFront was created.");

  const sheets = await front.getStyleSheets();
  ok(sheets, "getStyleSheets() succeeded even with documentless iframes.");

  // Bug 285395 limits the number of nested iframes to 10. There's one sheet per
  // frame so we should get 10 sheets. However, the limit might change in the
  // future so it's better not to rely on the limit. Asserting > 2 ensures that
  // the test page is actually loading nested iframes and this test is doing
  // something sensible (if we got this far, the test has served its purpose).
  ok(sheets.length > 2, sheets.length + " sheets found (expected 3 or more).");

  await client.close();
});
