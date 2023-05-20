/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Ensure that same-origin errors are logged to the console.

// XPCNativeWrapper is not defined globally in ESLint as it may be going away.
// See bug 1481337.
/* global XPCNativeWrapper */

"use strict";

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/test-checkloaduri-failure.html";

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const targetURL = "file:///something-weird";
  const onErrorMessage = waitForMessageByType(
    hud,
    "may not load or link",
    ".error"
  );
  SpecialPowers.spawn(gBrowser.selectedBrowser, [targetURL], url => {
    XPCNativeWrapper.unwrap(content).testImage(url);
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
