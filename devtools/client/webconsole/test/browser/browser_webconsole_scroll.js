/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = `data:text/html;charset=utf-8,<!DOCTYPE html><p>Web Console test for  scroll.</p>
  <script>
    var a = () => b();
    var b = () => c();
    var c = (i) => console.trace("trace in C " + i);

    for (let i = 0; i <= 100; i++) {
      console.log("init-" + i);
      if (i % 10 === 0) {
        c(i);
      }
    }
  </script>
`;

const { MESSAGE_SOURCE } = require("devtools/client/webconsole/constants");

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { ui } = hud;
  const outputContainer = ui.outputNode.querySelector(".webconsole-output");

  info("Console should be scrolled to bottom on initial load from page logs");
  await waitFor(() => findConsoleAPIMessage(hud, "init-100"));
  ok(hasVerticalOverflow(outputContainer), "There is a vertical overflow");
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info("Wait until all stacktraces are rendered");
  await waitFor(() => allTraceMessagesAreExpanded(hud));
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  await reloadBrowser();

  info("Console should be scrolled to bottom after refresh from page logs");
  await waitFor(() => findConsoleAPIMessage(hud, "init-100"));
  ok(hasVerticalOverflow(outputContainer), "There is a vertical overflow");
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info("Wait until all stacktraces are rendered");
  await waitFor(() => allTraceMessagesAreExpanded(hud));

  // There's an annoying race here where the SmartTrace from above goes into
  // the DOM, our waitFor passes, but the SmartTrace still hasn't called its
  // onReady callback. If this happens, it will call ConsoleOutput's
  // maybeScrollToBottomMessageCallback *after* we set scrollTop below,
  // causing it to undo our work. Waiting a little bit here should resolve it.
  await new Promise(r =>
    window.requestAnimationFrame(() => TestUtils.executeSoon(r))
  );
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info("Scroll up and wait for the layout to stabilize");
  outputContainer.scrollTop = 0;
  await new Promise(r =>
    window.requestAnimationFrame(() => TestUtils.executeSoon(r))
  );

  info("Add a console.trace message to check that the scroll isn't impacted");
  let onMessage = waitForMessageByType(hud, "trace in C", ".console-api");
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
  await executeAndWaitForResultMessage(hud, "21 + 21", "42");
  ok(hasVerticalOverflow(outputContainer), "There is a vertical overflow");
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info("Scroll up and wait for the layout to stabilize");
  outputContainer.scrollTop = 0;
  await new Promise(r =>
    window.requestAnimationFrame(() => TestUtils.executeSoon(r))
  );

  info(
    "Trigger a network request so the last message in the console store won't be visible"
  );
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function() {
    await content.fetch(
      "http://mochi.test:8888/browser/devtools/client/webconsole/test/browser/sjs_cors-test-server.sjs",
      { mode: "cors" }
    );
  });

  // Wait until the evalation result message isn't the last in the store anymore
  await waitFor(() => {
    const state = ui.wrapper.getStore().getState();
    return (
      state.messages.mutableMessagesById.get(state.messages.lastMessageId)
        ?.source === MESSAGE_SOURCE.NETWORK
    );
  });

  // Wait a bit so the pin to bottom would have the chance to be hit.
  await wait(500);
  ok(
    !isScrolledToBottom(outputContainer),
    "The console is not scrolled to the bottom"
  );

  info(
    "Evaluate a new command to check that the console scrolls to the bottom"
  );
  await executeAndWaitForResultMessage(hud, "7 + 2", "9");
  ok(
    isScrolledToBottom(outputContainer),
    "The console is scrolled to the bottom"
  );

  info(
    "Add a message to check that the console do scroll since we're at the bottom"
  );
  onMessage = waitForMessageByType(hud, "scroll", ".console-api");
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
  message = await executeAndWaitForResultMessage(
    hud,
    `
    x = new Error("myErrorObject");
    x.stack = "a@b/c.js:1:2\\nd@e/f.js:3:4";
    x;`,
    "myErrorObject"
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
  message = await executeAndWaitForErrorMessage(
    hud,
    `
      x = new Error("myEvaluatedThrownErrorObject");
      x.stack = "a@b/c.js:1:2\\nd@e/f.js:3:4";
      throw x;
    `,
    "Uncaught Error: myEvaluatedThrownErrorObject"
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
  message = await executeAndWaitForErrorMessage(
    hud,
    `
    setTimeout(() => {
      x = new Error("myThrownErrorObject");
      x.stack = "a@b/c.js:1:2\\nd@e/f.js:3:4";
      throw x
    }, 10)`,
    "Uncaught Error: myThrownErrorObject"
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
  onMessage = waitForMessageByType(hud, "trace in C", ".console-api");
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
  onMessage = waitForMessageByType(hud, "repeat", ".console-api");
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
  onMessage = waitForMessageByType(hud, "after repeat", ".console-api");
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
  const result = await executeAndWaitForResultMessage(
    hud,
    `Array.from({length: 100}, (_, i) => i)
      .reduce(
        (acc, item) => {acc["item-" + item] = item; return acc;},
        {}
      )`,
    "Object"
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

  await clearOutput(hud);
  // Log a big object that will be much larger than the output container
  onMessage = waitForMessageByType(hud, "WE ALL LIVE IN A", ".warn");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    const win = content.wrappedJSObject;
    for (let i = 1; i < 100; i++) {
      win["a" + i] = function(j) {
        win["a" + j]();
      }.bind(null, i + 1);
    }
    win.a100 = function() {
      win.console.warn(new Error("WE ALL LIVE IN A"));
    };
    win.a1();
  });
  message = await onMessage;
  // Give the intersection observer a chance to break this if it's going to
  await wait(500);
  // Assert here and below for ease of debugging where we lost the scroll
  is(
    isScrolledToBottom(outputContainer),
    true,
    "The output was scrolled to the bottom"
  );
  // Then log something else to make sure we haven't lost our scroll pinning
  onMessage = waitForMessageByType(hud, "YELLOW SUBMARINE", ".console-api");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], function() {
    content.wrappedJSObject.console.log("YELLOW SUBMARINE");
  });
  message = await onMessage;
  // Again, give the scroll position a chance to be broken
  await wait(500);
  is(
    isScrolledToBottom(outputContainer),
    true,
    "The output was scrolled to the bottom"
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

// This validates that 1) the last trace exists, and 2) that all *shown* traces
// are expanded. Traces that have been scrolled out of existence due to
// LazyMessageList are disregarded.
function allTraceMessagesAreExpanded(hud) {
  return (
    findConsoleAPIMessage(hud, "trace in C 100") &&
    findConsoleAPIMessages(hud, "trace in C").every(m =>
      m.querySelector(".frames")
    )
  );
}
