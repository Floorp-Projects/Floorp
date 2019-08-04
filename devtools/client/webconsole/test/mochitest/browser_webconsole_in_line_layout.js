/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the in-line layout works as expected

const TEST_URI =
  "data:text/html,<meta charset=utf8>Test in-line console layout";

const MINIMUM_MESSAGE_HEIGHT = 19;

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { ui } = hud;
  const { document } = ui;
  const appNode = document.querySelector(".webconsole-app");
  const filterBarNode = appNode.querySelector(
    ".webconsole-filteringbar-wrapper"
  );
  const outputNode = appNode.querySelector(".webconsole-output");
  const inputNode = appNode.querySelector(".jsterm-input-container");

  testLayout(appNode);

  is(outputNode.offsetHeight, 0, "output node has no height");
  is(
    filterBarNode.offsetHeight + inputNode.offsetHeight,
    appNode.offsetHeight,
    "The entire height is taken by filter bar and input"
  );

  info("Logging a message in the content window");
  const onLogMessage = waitForMessage(hud, "simple text message");
  ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.console.log("simple text message");
  });
  const logMessage = await onLogMessage;
  testLayout(appNode);
  is(
    outputNode.clientHeight,
    logMessage.node.clientHeight,
    "Output node is only the height of the message it contains"
  );

  info("Logging multiple messages to make the output overflow");
  const onLastMessage = waitForMessage(hud, "message-100");
  ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    for (let i = 1; i <= 100; i++) {
      content.wrappedJSObject.console.log("message-" + i);
    }
  });
  await onLastMessage;
  ok(
    outputNode.scrollHeight > outputNode.clientHeight,
    "Output node overflows"
  );
  testLayout(appNode);

  info("Make sure setting a tall value in the input does not break the layout");
  setInputValue(hud, "multiline\n".repeat(200));
  is(
    outputNode.clientHeight,
    MINIMUM_MESSAGE_HEIGHT,
    "One message is still visible in the output node"
  );
  testLayout(appNode);

  const filterBarHeight = filterBarNode.clientHeight;

  info("Shrink the window so the filter buttons are put in a new line");
  const toolbox = hud.ui.wrapper.toolbox;
  const hostWindow = toolbox.win.parent;
  hostWindow.resizeTo(300, window.screen.availHeight);
  await waitFor(() =>
    document.querySelector(".webconsole-filteringbar-wrapper.narrow")
  );

  ok(filterBarNode.clientHeight > filterBarHeight, "The filter bar is taller");
  testLayout(appNode);

  info("Expand the window so filter buttons aren't on their own line anymore");
  hostWindow.resizeTo(window.screen.availWidth, window.screen.availHeight);
  await waitFor(() =>
    document.querySelector(".webconsole-filteringbar-wrapper.wide")
  );
  testLayout(appNode);

  setInputValue(hud, "");
  testLayout(appNode);

  ui.clearOutput();
  testLayout(appNode);
  is(outputNode.offsetHeight, 0, "output node has no height");
  is(
    filterBarNode.offsetHeight + inputNode.offsetHeight,
    appNode.offsetHeight,
    "The entire height is taken by filter bar and input"
  );
});

function testLayout(node) {
  is(
    node.offsetHeight,
    node.scrollHeight,
    "there's no scrollbar on the wrapper"
  );
  ok(
    node.offsetHeight <= node.ownerDocument.body.offsetHeight,
    "console is not taller than document body"
  );
  const childSumHeight = [...node.childNodes].reduce(
    (height, n) => height + n.offsetHeight,
    0
  );
  ok(
    node.offsetHeight >= childSumHeight,
    "the sum of the height of wrapper child nodes is not taller than wrapper's one"
  );
}
