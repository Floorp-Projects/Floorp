/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// The target front holds resources that happend before ResourceCommand addeed listeners.
// Test whether that feature works correctly or not.
const TEST_URI =
  "https://example.com/browser/devtools/client/framework/test/doc_cached-resource.html";
const PARENT_MESSAGE = "Hello from parent";
const CHILD_MESSAGE = "Hello from child";

Services.scriptloader.loadSubScript(
  "chrome://mochitests/content/browser/devtools/client/webconsole/test/browser/shared-head.js",
  this
);

add_task(async function () {
  info("Open console");
  const tab = await addTab(TEST_URI);
  const toolbox = await openToolboxForTab(tab, "webconsole");
  const hud = toolbox.getCurrentPanel().hud;

  info("Check the initial messages");
  ok(
    findMessageByType(hud, PARENT_MESSAGE, ".console-api"),
    "Message from parent document is in console"
  );
  ok(
    findMessageByType(hud, CHILD_MESSAGE, ".console-api"),
    "Message from child document is in console"
  );

  info("Clear the messages");
  hud.ui.window.document.querySelector(".devtools-clear-icon").click();
  await waitUntil(
    () => !findMessageByType(hud, PARENT_MESSAGE, ".console-api")
  );

  info("Reload the browsing page");
  await navigateTo(TEST_URI);

  info("Check the messages after reloading");
  await waitUntil(
    () =>
      findMessageByType(hud, PARENT_MESSAGE, ".console-api") &&
      findMessageByType(hud, CHILD_MESSAGE, ".console-api")
  );
  ok(true, "All messages are shown correctly");
});
