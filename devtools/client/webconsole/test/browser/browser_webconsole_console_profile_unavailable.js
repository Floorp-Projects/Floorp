/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check console.profile() shows a warning with the new performance panel.
const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html><h1>test console.profile</h1>";

const EXPECTED_WARNING =
  "console.profile is not compatible with the new Performance recorder";

add_task(async function consoleProfileWarningWithNewPerfPanel() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Use console.profile in the content page");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.wrappedJSObject.console.profile();
  });

  await waitFor(
    () => findWarningMessage(hud, EXPECTED_WARNING),
    "Wait until the warning about console.profile is displayed"
  );
  ok(true, "The expected warning was displayed.");
});
