/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that autocomplete doesn't break when trying to reach into objects from
// a different domain. See Bug 989025.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/" +
  "test/browser/test-iframe-parent.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  await executeAndWaitForMessage(
    hud,
    "document.title",
    "iframe parent",
    ".result"
  );
  ok(true, "root document's title is accessible");

  // Make sure we don't throw when trying to autocomplete
  const autocompleteUpdated = hud.jsterm.once("autocomplete-updated");
  setInputValue(hud, "window[0].document");
  EventUtils.sendString(".");
  await autocompleteUpdated;

  setInputValue(hud, "window[0].document.title");
  const onPermissionDeniedMessage = waitForMessage(hud, "Permission denied");
  EventUtils.synthesizeKey("KEY_Enter");
  const permissionDenied = await onPermissionDeniedMessage;
  ok(
    permissionDenied.node.classList.contains("error"),
    "A message error is shown when trying to inspect window[0]"
  );

  await executeAndWaitForMessage(
    hud,
    "window.location",
    "test-iframe-parent.html",
    ".result"
  );
  ok(true, "root document's location is accessible");
});
