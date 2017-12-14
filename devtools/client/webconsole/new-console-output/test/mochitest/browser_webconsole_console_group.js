/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Check console.group, console.groupCollapsed and console.groupEnd calls
// behave as expected.

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/new-console-output/test/mochitest/test-console-group.html";
const { INDENT_WIDTH } = require("devtools/client/webconsole/new-console-output/components/MessageIndent");

add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);

  const store = hud.ui.newConsoleOutput.getStore();
  // Adding logging each time the store is modified in order to check
  // the store state in case of failure.
  store.subscribe(() => {
    const messages = [...store.getState().messages.messagesById]
      .reduce(function (res, [id, message]) {
        res.push({
          id,
          type: message.type,
          parameters: message.parameters,
          messageText: message.messageText
        });
        return res;
      }, []);
    info("messages : " + JSON.stringify(messages));
  });

  const onMessagesLogged = waitForMessage(hud, "log-6");
  ContentTask.spawn(gBrowser.selectedBrowser, null, function () {
    content.wrappedJSObject.doLog();
  });
  await onMessagesLogged;

  info("Test a group at root level");
  let node = findMessage(hud, "group-1");
  testClass(node, "startGroup");
  testIndent(node, 0);
  testGroupToggle({
    node,
    store,
    shouldBeOpen: true,
    visibleMessageIdsAfterExpand: ["1","2","3","4","6","8","9","12"],
    visibleMessageIdsAfterCollapse: ["1","8","9","12"],
  });

  info("Test a message in a 1 level deep group");
  node = findMessage(hud, "log-1");
  testClass(node, "log");
  testIndent(node, 1);

  info("Test a group in a 1 level deep group");
  node = findMessage(hud, "group-2");
  testClass(node, "startGroup");
  testIndent(node, 1);
  testGroupToggle({
    node,
    store,
    shouldBeOpen: true,
    visibleMessageIdsAfterExpand: ["1","2","3","4","6","8","9","12"],
    visibleMessageIdsAfterCollapse: ["1","2","3","6","8","9","12"],
  });

  info("Test a message in a 2 level deep group");
  node = findMessage(hud, "log-2");
  testClass(node, "log");
  testIndent(node, 2);

  info("Test a message in a 1 level deep group, after closing a 2 level deep group");
  node = findMessage(hud, "log-3");
  testClass(node, "log");
  testIndent(node, 1);

  info("Test a message at root level, after closing all the groups");
  node = findMessage(hud, "log-4");
  testClass(node, "log");
  testIndent(node, 0);

  info("Test a collapsed group at root level");
  node = findMessage(hud, "group-3");
  testClass(node, "startGroupCollapsed");
  testIndent(node, 0);
  testGroupToggle({
    node,
    store,
    shouldBeOpen: false,
    visibleMessageIdsAfterExpand: ["1","2","3","4","6","8","9","10","12"],
    visibleMessageIdsAfterCollapse: ["1","2","3","4","6","8","9","12"]
  });

  info("Test a message at root level, after closing a collapsed group");
  node = findMessage(hud, "log-6");
  testClass(node, "log");
  testIndent(node, 0);
  let nodes = hud.ui.outputNode.querySelectorAll(".message");
  is(nodes.length, 8, "expected number of messages are displayed");
});

function testClass(node, className) {
  ok(node.classList.contains(className), `message has the expected "${className}" class`);
}

function testIndent(node, indent) {
  indent = `${indent * INDENT_WIDTH}px`;
  is(node.querySelector(".indent").style.width, indent,
    "message has the expected level of indentation");
}

function testGroupToggle({ node, store, shouldBeOpen,
  visibleMessageIdsAfterExpand, visibleMessageIdsAfterCollapse }) {
  let toggleArrow = node.querySelector(".theme-twisty");
  const isOpen = node => node.classList.contains("open");
  const assertVisibleMessageIds = (expanded) => {
    let visibleMessageIds = store.getState().messages.visibleMessages;
    expanded ? is(visibleMessageIds.toString(), visibleMessageIdsAfterExpand.toString()) :
      is(visibleMessageIds.toString(), visibleMessageIdsAfterCollapse.toString());
  }

  is(isOpen(node), shouldBeOpen);
  assertVisibleMessageIds(shouldBeOpen);

  toggleArrow.click();
  shouldBeOpen = !shouldBeOpen;
  is(isOpen(node), shouldBeOpen);
  assertVisibleMessageIds(shouldBeOpen);

  toggleArrow.click();
  shouldBeOpen = !shouldBeOpen;
  is(isOpen(node), shouldBeOpen);
  assertVisibleMessageIds(shouldBeOpen);
}