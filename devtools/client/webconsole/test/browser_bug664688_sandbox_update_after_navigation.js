/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests if the JSTerm sandbox is updated when the user navigates from one
// domain to another, in order to avoid permission denied errors with a sandbox
// created for a different origin.

"use strict";

add_task(function* () {
  const TEST_URI1 = "http://example.com/browser/devtools/client/webconsole/" +
                    "test/test-console.html";
  const TEST_URI2 = "http://example.org/browser/devtools/client/webconsole/" +
                    "test/test-console.html";

  yield loadTab(TEST_URI1);
  let hud = yield openConsole();

  hud.jsterm.clearOutput();
  hud.jsterm.execute("window.location.href");

  info("wait for window.location.href");

  let msgForLocation1 = {
    webconsole: hud,
    messages: [
      {
        name: "window.location.href jsterm input",
        text: "window.location.href",
        category: CATEGORY_INPUT,
      },
      {
        name: "window.location.href result is displayed",
        text: TEST_URI1,
        category: CATEGORY_OUTPUT,
      },
    ],
  };

  yield waitForMessages(msgForLocation1);

  // load second url
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, TEST_URI2);
  yield loadBrowser(gBrowser.selectedBrowser);

  is(hud.outputNode.textContent.indexOf("Permission denied"), -1,
     "no permission denied errors");

  hud.jsterm.clearOutput();
  hud.jsterm.execute("window.location.href");

  info("wait for window.location.href after page navigation");

  yield waitForMessages({
    webconsole: hud,
    messages: [
      {
        name: "window.location.href jsterm input",
        text: "window.location.href",
        category: CATEGORY_INPUT,
      },
      {
        name: "window.location.href result is displayed",
        text: TEST_URI2,
        category: CATEGORY_OUTPUT,
      },
    ],
  });

  is(hud.outputNode.textContent.indexOf("Permission denied"), -1,
     "no permission denied errors");

  // Navigation clears messages. Wait for that clear to happen before
  // continuing the test or it might destroy messages we wait later on (Bug
  // 1270234).
  let cleared = hud.jsterm.once("messages-cleared");

  gBrowser.goBack();

  info("Waiting for messages to be cleared due to navigation");
  yield cleared;

  info("Messages cleared after navigation; checking location");
  hud.jsterm.execute("window.location.href");

  info("wait for window.location.href after goBack()");
  yield waitForMessages(msgForLocation1);
  is(hud.outputNode.textContent.indexOf("Permission denied"), -1,
     "no permission denied errors");
});
