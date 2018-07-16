/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that autocomplete doesn't break when trying to reach into objects from
// a different domain. See Bug 989025.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-iframe-parent.html";

add_task(async function() {
  // Run test with legacy JsTerm
  await performTests();
  // And then run it with the CodeMirror-powered one.
  await pushPref("devtools.webconsole.jsterm.codeMirror", true);
  await performTests();
});

async function performTests() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { jsterm } = hud;

  const onParentTitle = waitForMessage(hud, "iframe parent");
  jsterm.execute("document.title");
  await onParentTitle;
  ok(true, "root document's title is accessible");

  // Make sure we don't throw when trying to autocomplete
  const autocompleteUpdated = hud.jsterm.once("autocomplete-updated");
  jsterm.setInputValue("window[0].document");
  EventUtils.sendString(".");
  await autocompleteUpdated;

  hud.jsterm.setInputValue("window[0].document.title");
  const onPermissionDeniedMessage = waitForMessage(hud, "Permission denied");
  EventUtils.synthesizeKey("KEY_Enter");
  const permissionDenied = await onPermissionDeniedMessage;
  ok(permissionDenied.node.classList.contains("error"),
    "A message error is shown when trying to inspect window[0]");

  const onParentLocation = waitForMessage(hud, "test-iframe-parent.html");
  hud.jsterm.execute("window.location");
  await onParentLocation;
  ok(true, "root document's location is accessible");
}
