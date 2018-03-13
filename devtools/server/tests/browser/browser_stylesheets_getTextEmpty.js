/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that StyleSheetActor.getText handles empty text correctly.

const {StyleSheetsFront} = require("devtools/shared/fronts/stylesheets");

const CONTENT = "<style>body { background-color: #f06; }</style>";
const TEST_URI = "data:text/html;charset=utf-8," + encodeURIComponent(CONTENT);

add_task(async function() {
  await addTab(TEST_URI);

  info("Initialising the debugger server and client.");
  initDebuggerServer();
  let client = new DebuggerClient(DebuggerServer.connectPipe());
  let form = await connectDebuggerClient(client);

  info("Attaching to the active tab.");
  await client.attachTab(form.actor);

  let front = StyleSheetsFront(client, form);
  ok(front, "The StyleSheetsFront was created.");

  let sheets = await front.getStyleSheets();
  ok(sheets, "getStyleSheets() succeeded");
  is(sheets.length, 1,
     "getStyleSheets() returned the correct number of sheets");

  let sheet = sheets[0];
  await sheet.update("", false);
  let longStr = await sheet.getText();
  let source = await longStr.string();
  is(source, "", "text is empty");

  await client.close();
});
