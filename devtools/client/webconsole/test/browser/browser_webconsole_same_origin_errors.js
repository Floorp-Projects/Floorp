/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure that same-origin errors are logged to the console.

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/test-same-origin-required-load.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  const targetURL = "http://example.org";
  const onErrorMessage = waitForMessage(hud, "may not load data");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [targetURL], url => {
    XPCNativeWrapper.unwrap(content).testTrack(url);
  });
  const message = await onErrorMessage;
  const node = message.node;
  ok(
    node.classList.contains("error"),
    "The message has the expected classname"
  );
  ok(
    node.textContent.includes(targetURL),
    "The message is about the thing we were expecting"
  );
});
