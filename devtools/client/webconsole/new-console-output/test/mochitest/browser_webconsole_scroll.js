/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>Web Console test for " +
                 "scroll behavior";
add_task(async function () {
  const hud = await openNewTabAndConsole(TEST_URI);
  let {ui} = hud;

  info("Waiting for logged messages");

  let receievedMessages = waitForMessages({hud, messages: [{
    text: "init-99"
  }]});
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    for (let i = 0; i < 100; i++) {
      content.wrappedJSObject.console.log("init-" + i);
    }
  });
  await receievedMessages;

  const outputContainer = ui.outputNode.querySelector(".webconsole-output");
  ok(outputContainer.scrollHeight > outputContainer.clientHeight,
    "There is a vertical overflow");

  info("Scroll up");
  outputContainer.scrollTop = 0;

  info("Add a message to check that the scroll isn't impacted");
  receievedMessages = waitForMessages({hud, messages: [{
    text: "stay"
  }]});
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    content.wrappedJSObject.console.log("stay");
  });
  await receievedMessages;
  is(outputContainer.scrollTop, 0, "The console stayed scrolled to the top");

  info("Evaluate a command to check that the console scrolls to the bottom");
  receievedMessages = waitForMessages({hud, messages: [{
    text: "42"
  }]});
  ui.jsterm.execute("21 + 21");
  await receievedMessages;
  is(isScrolledToBottom(outputContainer), true, "The console is scrolled to the bottom");

  info("Add a message to check that the console do scroll since we're at the bottom");
  receievedMessages = waitForMessages({hud, messages: [{
    text: "scroll"
  }]});
  ContentTask.spawn(gBrowser.selectedBrowser, {}, function () {
    content.wrappedJSObject.console.log("scroll");
  });
  await receievedMessages;
  is(isScrolledToBottom(outputContainer), true, "The console is scrolled to the bottom");
});

function isScrolledToBottom(container) {
  if (!container.lastChild) {
    return true;
  }
  let lastNodeHeight = container.lastChild.clientHeight;
  return container.scrollTop + container.clientHeight >=
         container.scrollHeight - lastNodeHeight / 2;
}
