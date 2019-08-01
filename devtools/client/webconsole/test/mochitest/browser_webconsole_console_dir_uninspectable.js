/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Make sure that the Web Console output does not break after we try to call
// console.dir() for objects that are not inspectable.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,test console.dir on uninspectable object";
const FIRST_LOG_MESSAGE = "fooBug773466a";
const SECOND_LOG_MESSAGE = "fooBug773466b";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;

  info("Logging a first message to make sure everything is working");
  let onLogMessage = waitForMessage(hud, FIRST_LOG_MESSAGE);
  jsterm.execute(`console.log("${FIRST_LOG_MESSAGE}")`);
  await onLogMessage;

  info("console.dir on an uninspectable object");
  const onDirMessage = waitForMessage(hud, "Object {  }");
  jsterm.execute("console.dir(Object.create(null))");
  await onDirMessage;

  info("Logging a second message to make sure the console is not broken");
  onLogMessage = waitForMessage(hud, SECOND_LOG_MESSAGE);
  // Logging from content to make sure the console API is working.
  ContentTask.spawn(gBrowser.selectedBrowser, SECOND_LOG_MESSAGE, string => {
    content.console.log(string);
  });
  await onLogMessage;

  ok(
    true,
    "The console.dir call on an uninspectable object did not break the console"
  );
});
