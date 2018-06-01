/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the cd() jsterm helper function works as expected. See bug 609872.

"use strict";

const TEST_URI = "http://example.com/browser/devtools/client/webconsole/" +
                 "test/mochitest/test-cd-iframe-parent.html";

add_task(async function() {
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Test initial state");
  await executeWindowTest(hud, "parent");

  info("cd() into the iframe using a selector");
  await hud.jsterm.execute(`cd("iframe")`);
  await executeWindowTest(hud, "child");

  info("cd() out of the iframe, reset to default window");
  await hud.jsterm.execute("cd()");
  await executeWindowTest(hud, "parent");

  info("cd() into the iframe using an iframe DOM element");
  await hud.jsterm.execute(`cd($("iframe"))`);
  await executeWindowTest(hud, "child");

  info("cd(window.parent)");
  await hud.jsterm.execute("cd(window.parent)");
  await executeWindowTest(hud, "parent");

  info("call cd() with unexpected arguments");
  let onCdErrorMessage = waitForMessage(hud, "Cannot cd()");
  hud.jsterm.execute("cd(document)");
  let cdErrorMessage = await onCdErrorMessage;
  ok(cdErrorMessage.node.classList.contains("error"),
    "An error message is shown when calling the cd command with `document`");

  onCdErrorMessage = waitForMessage(hud, "Cannot cd()");
  hud.jsterm.execute(`cd("p")`);
  cdErrorMessage = await onCdErrorMessage;
  ok(cdErrorMessage.node.classList.contains("error"),
    "An error message is shown when calling the cd command with a non iframe selector");
});

async function executeWindowTest(hud, iframeRole) {
  const BASE_TEXT = "Test for the cd() command (bug 609872) - iframe";
  const onMessages = waitForMessages({
    hud,
    messages: [{
      text: `${BASE_TEXT} ${iframeRole}`
    }, {
      text: `p: ${BASE_TEXT} ${iframeRole}`
    }, {
      text: `obj: ${iframeRole}!`
    }]
  });

  hud.jsterm.execute(`document.title`);
  hud.jsterm.execute(`"p: " + document.querySelector("p").textContent`);
  hud.jsterm.execute(`"obj: " + window.foobar`);

  const messages = await onMessages;
  ok(messages, `Expected evaluation result messages are shown in ${iframeRole} iframe`);

  // Clear the output so we don't pollute the next assertions.
  hud.jsterm.clearOutput();
}
