/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>Web Console test for splitting";

add_task(async function() {
  info("Test various cases where the escape key should hide the split console.");

  let toolbox = await openNewTabAndToolbox(TEST_URI, "inspector");

  info("Send ESCAPE key and wait for the split console to be displayed");

  let onSplitConsoleReady = toolbox.once("webconsole-ready");
  toolbox.win.focus();
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await onSplitConsoleReady;

  let hud = toolbox.getPanel("webconsole").hud;
  let jsterm = hud.jsterm;
  ok(toolbox.splitConsole, "Split console is created.");

  info("Wait for the autocomplete to show suggestions for `document.location.`");
  let popup = jsterm.autocompletePopup;
  let onPopupShown = popup.once("popup-opened");
  jsterm.focus();
  jsterm.setInputValue("document.location.");
  EventUtils.sendKey("TAB", hud.iframeWindow);
  await onPopupShown;

  info("Send ESCAPE key and check that it only hides the autocomplete suggestions");

  let onPopupClosed = popup.once("popup-closed");
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await onPopupClosed;

  ok(!popup.isOpen, "Auto complete popup is hidden.");
  ok(toolbox.splitConsole, "Split console is open after hiding the autocomplete popup.");

  info("Send ESCAPE key again and check that now closes the splitconsole");
  let onSplitConsoleEvent = toolbox.once("split-console");
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await onSplitConsoleEvent;

  ok(!toolbox.splitConsole, "Split console is hidden.");
});
