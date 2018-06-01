/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI = "data:text/html;charset=utf-8,<p>Web Console test for splitting";

add_task(async function() {
  info("Test various cases where the escape key should hide the split console.");

  const toolbox = await openNewTabAndToolbox(TEST_URI, "inspector");

  info("Send ESCAPE key and wait for the split console to be displayed");

  const onSplitConsoleReady = toolbox.once("webconsole-ready");
  toolbox.win.focus();
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await onSplitConsoleReady;

  const hud = toolbox.getPanel("webconsole").hud;
  const jsterm = hud.jsterm;
  ok(toolbox.splitConsole, "Split console is created.");

  info("Wait for the autocomplete to show suggestions for `document.location.`");
  const popup = jsterm.autocompletePopup;
  const onPopupShown = popup.once("popup-opened");
  jsterm.focus();
  jsterm.setInputValue("document.location.");
  EventUtils.sendKey("TAB", hud.iframeWindow);
  await onPopupShown;

  info("Send ESCAPE key and check that it only hides the autocomplete suggestions");

  const onPopupClosed = popup.once("popup-closed");
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await onPopupClosed;

  ok(!popup.isOpen, "Auto complete popup is hidden.");
  ok(toolbox.splitConsole, "Split console is open after hiding the autocomplete popup.");

  info("Send ESCAPE key again and check that now closes the splitconsole");
  const onSplitConsoleEvent = toolbox.once("split-console");
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await onSplitConsoleEvent;

  ok(!toolbox.splitConsole, "Split console is hidden.");
});
