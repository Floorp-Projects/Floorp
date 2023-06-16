/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that filters don't affect navigation markers.

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html>
  <p>Web Console test for navigation marker filtering.</p>
  <script>console.log("hello " + "world");</script>`;

add_task(async function () {
  // Enable persist log
  await pushPref("devtools.webconsole.persistlog", true);

  const hud = await openNewTabAndConsole(TEST_URI);
  await waitFor(
    () => findConsoleAPIMessage(hud, "hello world"),
    "Wait for log message to be rendered"
  );
  ok(true, "Log message rendered");

  info("Reload the page");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    content.location.reload();
  });

  // Wait for the navigation message to be displayed.
  await waitFor(
    () => findMessageByType(hud, "Navigated to", ".navigationMarker"),
    "Wait for navigation message to be rendered"
  );

  // Wait for 2 hellow world messages to be displayed.
  await waitFor(
    () => findConsoleAPIMessages(hud, "hello world").length == 2,
    "Wait for log message to be rendered after navigation"
  );

  info("disable all filters and set a text filter that doesn't match anything");
  await setFilterState(hud, {
    error: false,
    warn: false,
    log: false,
    info: false,
    text: "qwqwqwqwqwqw",
  });

  await waitFor(
    () => !findConsoleAPIMessage(hud, "hello world"),
    "Wait for the log messages to be hidden"
  );
  ok(
    findMessageByType(hud, "Navigated to", ".navigationMarker"),
    "The navigation marker is still visible"
  );

  info("Navigate to a different origin");
  let newUrl = `http://example.net/document-builder.sjs?html=HelloNet`;
  await navigateTo(newUrl);
  // Wait for the navigation message to be displayed.
  await waitFor(
    () => findMessageByType(hud, "Navigated to " + newUrl, ".navigationMarker"),
    "Wait for example.net navigation message to be rendered"
  );
  ok(true, "Navigation message for example.net was displayed as expected");

  info("Navigate to another different origin");
  newUrl = `http://example.com/document-builder.sjs?html=HelloCom`;
  await navigateTo(newUrl);
  // Wait for the navigation message to be displayed.
  await waitFor(
    () => findMessageByType(hud, "Navigated to " + newUrl, ".navigationMarker"),
    "Wait for example.com navigation message to be rendered"
  );
  ok(true, "Navigation message for example.com was displayed as expected");
});
