/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Make sure that the Web Console output does not break after we try to call
// console.dir() for objects that are not inspectable.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html>test console.dir on uninspectable object";
const FIRST_LOG_MESSAGE = "fooBug773466a";
const SECOND_LOG_MESSAGE = "fooBug773466b";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Logging a first message to make sure everything is working");
  await executeAndWaitForMessageByType(
    hud,
    `console.log("${FIRST_LOG_MESSAGE}")`,
    FIRST_LOG_MESSAGE,
    ".console-api"
  );

  info("console.dir on an uninspectable object");
  await executeAndWaitForMessageByType(
    hud,
    "console.dir(Object.create(null))",
    "Object {  }",
    ".console-api"
  );

  info("Logging a second message to make sure the console is not broken");
  const onLogMessage = waitForMessageByType(
    hud,
    SECOND_LOG_MESSAGE,
    ".console-api"
  );
  // Logging from content to make sure the console API is working.
  SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [SECOND_LOG_MESSAGE],
    string => {
      content.console.log(string);
    }
  );
  await onLogMessage;

  ok(
    true,
    "The console.dir call on an uninspectable object did not break the console"
  );
});
