/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that screenshot command works properly

"use strict";

const TEST_URI = `data:text/html,<!DOCTYPE html><meta charset=utf8><script>
  function screenshot() {
    console.log("contextScreen");
  }
</script>`;

add_task(async function () {
  await addTab(TEST_URI);

  const hud = await openConsole();
  ok(hud, "web console opened");

  await testCommand(hud);
  await testUserScreenshotFunction(hud);
});

async function testCommand(hud) {
  const command = `:screenshot --clipboard`;
  await executeAndWaitForMessageByType(
    hud,
    command,
    "Screenshot copied to clipboard.",
    ".console-api"
  );
  ok(true, ":screenshot was executed as expected");

  const helpMessage = await executeAndWaitForMessageByType(
    hud,
    `:screenshot --help`,
    "Save an image of the page",
    ".console-api"
  );
  ok(helpMessage, ":screenshot --help was executed as expected");
  is(
    helpMessage.node.innerText.match(/--\w+/g).join("\n"),
    [
      "--clipboard",
      "--delay",
      "--dpr",
      "--fullpage",
      "--selector",
      "--file",
      "--filename",
    ].join("\n"),
    "Help message references the arguments of the screenshot command"
  );
}

// if a user defines a screenshot, as is the case in the Test URI, the
// command should not overwrite the screenshot function
async function testUserScreenshotFunction(hud) {
  const command = `screenshot()`;
  await executeAndWaitForMessageByType(
    hud,
    command,
    "contextScreen",
    ".console-api"
  );
  ok(
    true,
    "content screenshot function is not overidden and was executed as expected"
  );
}
