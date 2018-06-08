/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that screenshot command works properly

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test_jsterm_screenshot_command.html";

const FileUtils = (ChromeUtils.import("resource://gre/modules/FileUtils.jsm", {})).FileUtils;
// on some machines, such as macOS, dpr is set to 2. This is expected behavior, however
// to keep tests consistant across OSs we are setting the dpr to 1
const dpr = "--dpr 1";

add_task(async function() {
  await addTab(TEST_URI);

  const hud = await openConsole();
  ok(hud, "web console opened");

  await testFile(hud);
});

async function testFile(hud) {
  // Test capture to file
  const file = FileUtils.getFile("TmpD", [ "TestScreenshotFile.png" ]);
  const command = `:screenshot ${file.path} ${dpr}`;
  const onMessage = waitForMessage(hud, `Saved to ${file.path}`);
  hud.jsterm.execute(command);
  await onMessage;

  ok(file.exists(), "Screenshot file exists");

  if (file.exists()) {
    file.remove(false);
  }
}

