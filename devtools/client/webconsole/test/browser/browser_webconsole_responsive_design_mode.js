/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that messages are displayed in the console when RDM is enabled

const TEST_URI =
  "data:text/html,<!DOCTYPE html><meta charset=utf8>Test logging in RDM";

add_task(async function() {
  const tab = await addTab(TEST_URI);

  info("Open responsive design mode");
  await openRDM(tab);

  info("Log a message before the console is open");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.console.log("Cached message");
  });

  info("Open the console");
  const hud = await openConsole(tab);
  await waitFor(
    () => findConsoleAPIMessage(hud, "Cached message"),
    "Cached message isn't displayed in the console output"
  );
  ok(true, "Cached message is displayed in the console");

  info("Log a message while the console is open");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.console.log("Live message");
  });

  await waitFor(
    () => findConsoleAPIMessage(hud, "Live message"),
    "Live message isn't displayed in the console output"
  );
  ok(true, "Live message is displayed in the console");

  info("Close responsive design mode");
  await closeRDM(tab);
});
