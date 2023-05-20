/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check that calling deprecated getter displays a deprecation warning.
const TEST_URI =
  "data:text/html;charset=utf8,<!DOCTYPE html><h1>Deprecation warning";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const deprecatedWarningMessageText = "mozPressure is deprecated";

  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.testMouseEvent = new content.MouseEvent("click");
    content.wrappedJSObject.console.log("oi-test", content.testMouseEvent);
  });
  const node = await waitFor(() => findConsoleAPIMessage(hud, "oi-test"));

  info("Expand the MouseEvent object");
  const oi = node.querySelector(".tree");
  expandObjectInspectorNode(oi);
  await waitFor(() => getObjectInspectorNodes(oi).length > 1);

  info("Wait for a bit so any warning message could be displayed");
  await wait(1000);
  ok(
    !findWarningMessage(hud, deprecatedWarningMessageText, ".warn"),
    "Expanding the MouseEvent object didn't triggered the deprecation warning"
  );

  info("Access the deprecated getter to trigger a warning message");
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.testMouseEvent.mozPressure;
  });

  await waitFor(() =>
    findWarningMessage(hud, deprecatedWarningMessageText, ".warn")
  );
  ok(
    true,
    "Calling the mozPressure getter did triggered the deprecation warning"
  );

  info("Clear the console and access the deprecated getter again");
  await clearOutput(hud);
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], function () {
    content.testMouseEvent.mozPressure;
  });
  info("Wait for a bit so any warning message could be displayed");
  await wait(1000);
  ok(
    !findWarningMessage(hud, deprecatedWarningMessageText, ".warn"),
    "Calling the mozPressure getter a second time did not trigger the deprecation warning again"
  );
});
