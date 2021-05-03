/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// The target front holds resources that happend before ResourceCommand addeed listeners.
// Test whether that feature works correctly or not.
const TEST_URI =
  "http://example.com/browser/devtools/client/framework/test/doc_cached-resource.html";
const PARENT_MESSAGE = "Hello from parent";
const CHILD_MESSAGE = "Hello from child";

add_task(async function() {
  info("Open console");
  const tab = await addTab(TEST_URI);
  const toolbox = await openToolboxForTab(tab, "webconsole");
  const hud = toolbox.getCurrentPanel().hud;

  info("Check the initial messages");
  ok(
    findMessage(hud, PARENT_MESSAGE),
    "Message from parent doument is in console"
  );
  ok(
    findMessage(hud, CHILD_MESSAGE),
    "Message from child doument is in console"
  );

  info("Clear the messages");
  hud.ui.window.document.querySelector(".devtools-clear-icon").click();
  await waitUntil(() => !findMessage(hud, PARENT_MESSAGE));

  info("Reload the browsing page");
  await navigateTo(TEST_URI);

  info("Check the messages after reloading");
  await waitUntil(
    () => findMessage(hud, PARENT_MESSAGE) && findMessage(hud, CHILD_MESSAGE)
  );
  ok(true, "All messages are shown correctly");
});

function findMessage(hud, text, selector = ".message") {
  const messages = hud.ui.outputNode.querySelectorAll(selector);
  return Array.prototype.find.call(messages, el =>
    el.textContent.includes(text)
  );
}
