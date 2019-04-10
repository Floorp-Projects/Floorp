/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const MESSAGES = [
  "123-456-7890",
  "foo@bar.com",
  "http://abc.com/q?fizz=buzz&alpha=beta/",
  "https://xyz.com/?path=/world",
  "foooobaaaar",
  "123 working",
];

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                  "test/mochitest/test-console-filter-by-regex-input.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);
  const { outputNode } = hud.ui;

  await waitFor(() => findMessage(hud, MESSAGES[5]), null, 200);

  let filteredNodes;

  info("Filter out messages that begin with numbers");
  await setFilterInput(hud, "/^[0-9]/", MESSAGES[5]);
  filteredNodes = outputNode.querySelectorAll(".message");
  checkFilteredMessages(filteredNodes, [
    MESSAGES[0],
    MESSAGES[5],
  ], 2);

  info("Filter out messages that are phone numbers");
  await setFilterInput(hud, "/\\d{3}\\-\\d{3}\\-\\d{4}/", MESSAGES[0]);
  filteredNodes = outputNode.querySelectorAll(".message");
  checkFilteredMessages(filteredNodes, [MESSAGES[0]], 1);

  info("Filter out messages that are an email address");
  await setFilterInput(hud, "/^\\w+@[a-zA-Z]+\\.[a-zA-Z]{2,3}$/", MESSAGES[1]);
  filteredNodes = outputNode.querySelectorAll(".message");
  checkFilteredMessages(filteredNodes, [MESSAGES[1]], 1);

  info("Filter out messages that contain query strings");
  await setFilterInput(hud, "/\\?([^=&]+=[^=&]+&?)*\\//", MESSAGES[2]);
  filteredNodes = outputNode.querySelectorAll(".message");
  checkFilteredMessages(filteredNodes, [MESSAGES[2]], 1);

  // If regex is invalid, do a normal text search instead
  info("Filter messages using normal text search if regex is invalid");
  await setFilterInput(hud, "/?path=/", MESSAGES[3]);
  filteredNodes = outputNode.querySelectorAll(".message");
  checkFilteredMessages(filteredNodes, [MESSAGES[3]], 1);

  info("Filter out messages not ending with numbers");
  await setFilterInput(hud, "/[^0-9]$/", MESSAGES[5]);
  filteredNodes = outputNode.querySelectorAll(".message");
  checkFilteredMessages(filteredNodes, [
    MESSAGES[1],
    MESSAGES[2],
    MESSAGES[3],
    MESSAGES[4],
    MESSAGES[5],
  ], 5);
});

async function setFilterInput(hud, value, lastMessage) {
  hud.ui.filterBox.focus();
  hud.ui.filterBox.select();
  EventUtils.sendString(value);
  await waitFor(() => findMessage(hud, lastMessage), null, 200);
}

function checkFilteredMessages(filteredNodes, expectedMessages, expectedCount) {
  is(filteredNodes.length, expectedCount,
    `${expectedCount} messages should be displayed`);

  filteredNodes.forEach((node, id) => {
    const messageBody = node.querySelector(".message-body").textContent;
    ok(messageBody, expectedMessages[id]);
  });
}
