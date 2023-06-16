/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// XXX Remove this when the file is migrated to the new frontend.
/* eslint-disable no-undef */

// Check that console.clear() does not clear the output of the browser console.

"use strict";

const TEST_URI = "data:text/html;charset=utf8,<!DOCTYPE html><p>Bug 1296870";

add_task(async function () {
  await loadTab(TEST_URI);
  const hud = await BrowserConsoleManager.toggleBrowserConsole();

  info("Log a new message from the content page");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.wrappedJSObject.console.log("msg");
  });
  await waitForMessageByType(hud, "msg", ".console-api");

  info("Send a console.clear() from the content page");
  SpecialPowers.spawn(gBrowser.selectedBrowser, [], async function () {
    content.wrappedJSObject.console.clear();
  });
  await waitForMessageByType(hud, "Console was cleared", ".console-api");

  info(
    "Check that the messages logged after the first clear are still displayed"
  );
  ok(hud.ui.outputNode.textContent.includes("msg"), "msg is in the output");
});
