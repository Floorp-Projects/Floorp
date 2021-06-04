/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<p>Web Console test for  scroll.</p>
  <script>
    var a = () => b();
    var b = () => c();
    var c = () => console.trace("trace in C");

    for (let i = 0; i < 100; i++) {
      if (i % 10 === 0) {
        c();
      }
      console.log("init-" + i);
    }
  </script>
`;
add_task(async function() {
  // Disable bfcache for Fission for now.
  // If Fission is disabled, the pref is no-op.
  await SpecialPowers.pushPrefEnv({
    set: [["fission.bfcacheInParent", false]],
  });

  const hud = await openNewTabAndConsole(TEST_URI);
  const { ui } = hud;
  const outputContainer = ui.outputNode.querySelector(".webconsole-output");

  info("Console should be scrolled to bottom on initial load from page logs");
  await waitFor(() => findMessage(hud, "init-99"));
  ok(hasVerticalOverflow(outputContainer), "There is a vertical overflow");
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info("Wait until all stacktraces are rendered");
  await waitFor(
    () => outputContainer.querySelectorAll(".frames").length === 10
  );
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  await refreshTab();

  info("Console should be scrolled to bottom after refresh from page logs");
  await waitFor(() => findMessage(hud, "init-99"));
  ok(hasVerticalOverflow(outputContainer), "There is a vertical overflow");
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info("Wait until all stacktraces are rendered");
  await waitFor(
    () => outputContainer.querySelectorAll(".frames").length === 10
  );
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info("Scroll up");
  outputContainer.scrollTop = 0;

  info("Add a console.trace message to check that the scroll isn't impacted");
  let onMessage = waitForMessage(hud, "trace in C");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.c();
  });
  let message = await onMessage;
  ok(hasVerticalOverflow(outputContainer), "There is a vertical overflow");
  is(outputContainer.scrollTop, 0, "The console stayed scrolled to the top");

  info("Wait until the stacktrace is rendered");
  await waitFor(() => message.node.querySelector(".frame"));
  is(outputContainer.scrollTop, 0, "The console stayed scrolled to the top");

  info("Evaluate a command to check that the console scrolls to the bottom");
  await executeAndWaitForMessage(hud, "21 + 21", "42", ".result");
  ok(hasVerticalOverflow(outputContainer), "There is a vertical overflow");
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info(
    "Add a message to check that the console do scroll since we're at the bottom"
  );
  onMessage = waitForMessage(hud, "scroll");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.console.log("scroll");
  });
  await onMessage;
  ok(hasVerticalOverflow(outputContainer), "There is a vertical overflow");
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info(
    "Evaluate an Error object to check that the console scrolls to the bottom"
  );
  message = await executeAndWaitForMessage(
    hud,
    `
    x = new Error("myErrorObject");
    x.stack = "a@b/c.js:1:2\\nd@e/f.js:3:4";
    x;`,
    "myErrorObject",
    ".result"
  );
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info(
    "Wait until the stacktrace is rendered and check the console is scrolled"
  );
  await waitFor(() =>
    message.node.querySelector(".objectBox-stackTrace .frame")
  );
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info(
    "Throw an Error object in a direct evaluation to check that the console scrolls to the bottom"
  );
  message = await executeAndWaitForMessage(
    hud,
    `
      x = new Error("myEvaluatedThrownErrorObject");
      x.stack = "a@b/c.js:1:2\\nd@e/f.js:3:4";
      throw x;
    `,
    "Uncaught Error: myEvaluatedThrownErrorObject",
    ".error"
  );
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info(
    "Wait until the stacktrace is rendered and check the console is scrolled"
  );
  await waitFor(() =>
    message.node.querySelector(".objectBox-stackTrace .frame")
  );
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info("Throw an Error object to check that the console scrolls to the bottom");
  message = await executeAndWaitForMessage(
    hud,
    `
    setTimeout(() => {
      x = new Error("myThrownErrorObject");
      x.stack = "a@b/c.js:1:2\\nd@e/f.js:3:4";
      throw x
    }, 10)`,
    "Uncaught Error: myThrownErrorObject",
    ".error"
  );
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info(
    "Wait until the stacktrace is rendered and check the console is scrolled"
  );
  await waitFor(() =>
    message.node.querySelector(".objectBox-stackTrace .frame")
  );
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info(
    "Add a console.trace message to check that the console stays scrolled to bottom"
  );
  onMessage = waitForMessage(hud, "trace in C");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.c();
  });
  message = await onMessage;
  ok(hasVerticalOverflow(outputContainer), "There is a vertical overflow");
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info("Wait until the stacktrace is rendered");
  await waitFor(() => message.node.querySelector(".frame"));
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info("Check that repeated messages don't prevent scroll to bottom");
  // We log a first message.
  onMessage = waitForMessage(hud, "repeat");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.console.log("repeat");
  });
  message = await onMessage;

  // And a second one. We can't log them at the same time since we batch redux actions,
  // and the message would already appear with the repeat badge, and the bug is
  // only triggered when the badge is rendered after the initial message rendering.
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.console.log("repeat");
  });
  await waitFor(() => message.node.querySelector(".message-repeats"));
  ok(
    isScrolledToBottom(outputContainer),
    "The console is still scrolled to the bottom when the repeat badge is added"
  );

  info(
    "Check that adding a message after a repeated message scrolls to bottom"
  );
  onMessage = waitForMessage(hud, "after repeat");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.console.log("after repeat");
  });
  message = await onMessage;
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom after a repeated message"
  );

  info(
    "Check that switching between editor and inline mode keep the output scrolled to bottom"
  );
  await toggleLayout(hud);
  // Wait until the output is scrolled to the bottom.
  await waitFor(
    () => isScrolledToBottom(outputContainer),
    "Output does not scroll to the bottom after switching to editor mode"
  );
  ok(
    true,
    "The console is scrolled to the bottom after switching to editor mode"
  );

  // Switching back to inline mode
  await toggleLayout(hud);
  // Wait until the output is scrolled to the bottom.
  await waitFor(
    () => isScrolledToBottom(outputContainer),
    "Output does not scroll to the bottom after switching back to inline mode"
  );
  ok(
    true,
    "The console is scrolled to the bottom after switching back to inline mode"
  );

  info(
    "Check that expanding a large object does not scroll the output to the bottom"
  );
  // Clear the output so we only have the object
  await clearOutput(hud);
  // Evaluate an object with a hundred properties
  const result = await executeAndWaitForMessage(
    hud,
    `Array.from({length: 100}, (_, i) => i)
      .reduce(
        (acc, item) => {acc["item-" + item] = item; return acc;},
        {}
      )`,
    "Object",
    ".message.result"
  );
  // Expand the object
  result.node.querySelector(".arrow").click();
  // Wait until we have 102 nodes (the root node, 100 properties + <prototype>)
  await waitFor(() => result.node.querySelectorAll(".node").length === 102);
  // wait for a bit to give time to the resize observer callback to be triggered
  await wait(500);
  ok(hasVerticalOverflow(outputContainer), "The output does overflow");
  is(
    isScrolledToBottom(outputContainer),
    false,
    "The output was not scrolled to the bottom"
  );
});

function hasVerticalOverflow(container) {
  return container.scrollHeight > container.clientHeight;
}

function isScrolledToBottom(container) {
  if (!container.lastChild) {
    return true;
  }
  const lastNodeHeight = container.lastChild.clientHeight;
  return (
    container.scrollTop + container.clientHeight >=
    container.scrollHeight - lastNodeHeight / 2
  );
}
