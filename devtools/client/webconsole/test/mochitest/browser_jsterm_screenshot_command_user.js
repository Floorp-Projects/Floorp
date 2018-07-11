/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that screenshot command works properly

"use strict";

const TEST_URI = `data:text/html,<meta charset=utf8><script>
  function screenshot() {
    console.log(\"screen\");
  }
</script>`;

add_task(async function() {
  await addTab(TEST_URI);

  const hud = await openConsole();
  ok(hud, "web console opened");

  await testCommand(hud);
  await testUserScreenshotFunction(hud);
});

async function testCommand(hud) {
  const command = `:screenshot --clipboard`;
  const onMessage = waitForMessage(hud, "Screenshot copied to clipboard.");
  hud.jsterm.execute(command);
  await onMessage;
  ok(true, ":screenshot was executed as expected");
}

// if a user defines a screenshot, as is the case in the Test URI, the
// command should not overwrite the screenshot function
async function testUserScreenshotFunction(hud) {
  const command = `screenshot()`;
  const onMessage = waitForMessage(hud, "screen");
  hud.jsterm.execute(command);
  await onMessage;
  ok(true, "content screenshot function is not overidden and was executed as expected");
}
