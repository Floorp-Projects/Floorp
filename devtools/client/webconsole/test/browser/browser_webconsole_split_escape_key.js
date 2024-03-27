/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,<!DOCTYPE html><p>Web Console test for splitting";

add_task(async function () {
  info(
    "Test various cases where the escape key should hide the split console."
  );

  const toolbox = await openNewTabAndToolbox(TEST_URI, "inspector");

  info("Send ESCAPE key and wait for the split console to be displayed");

  const onSplitConsoleReady = toolbox.once("webconsole-ready");
  toolbox.win.focus();
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await onSplitConsoleReady;

  const hud = toolbox.getPanel("webconsole").hud;
  const jsterm = hud.jsterm;
  ok(toolbox.splitConsole, "Split console is created.");

  info(
    "Wait for the autocomplete to show suggestions for `document.location.`"
  );
  const popup = jsterm.autocompletePopup;
  const onPopupShown = popup.once("popup-opened");
  jsterm.focus();
  EventUtils.sendString("document.location.");
  await onPopupShown;

  info(
    "Send ESCAPE key and check that it only hides the autocomplete suggestions"
  );

  const onPopupClosed = popup.once("popup-closed");
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await onPopupClosed;

  ok(!popup.isOpen, "Auto complete popup is hidden.");
  ok(
    toolbox.splitConsole,
    "Split console is open after hiding the autocomplete popup."
  );

  info("Send ESCAPE key again and check that now closes the splitconsole");
  const onSplitConsoleEvent = toolbox.once("split-console");
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await onSplitConsoleEvent;

  ok(!toolbox.splitConsole, "Split console is hidden.");

  info("Test if Split console Shortcut stops working when it's disabled.");

  info("Setting the Pref to false and sending ESCAPE key.");
  await pushPref("devtools.toolbox.splitconsole.enabled", false);
  // pushPref doesn't trigger _prefChanged of toolbox-options.js, so we invoke the toolbox setting update manually
  toolbox.updateIsSplitConsoleEnabled();
  const onSplitConsole = toolbox.once("split-console");
  const onTimeout = wait(1000).then(() => "TIMEOUT");
  EventUtils.sendKey("ESCAPE", toolbox.win);
  const raceResult = await Promise.race([onSplitConsole, onTimeout]);
  is(raceResult, "TIMEOUT", "split-console wasn't emitted");

  ok(!toolbox.splitConsole, "Split console didn't get Triggered.");

  info("Setting the Pref to true and sending ESCAPE key again.");
  await pushPref("devtools.toolbox.splitconsole.enabled", true);
  toolbox.updateIsSplitConsoleEnabled();
  const onSplitConsoleReadyAgain = toolbox.once("split-console");
  EventUtils.sendKey("ESCAPE", toolbox.win);
  await onSplitConsoleReadyAgain;

  ok(toolbox.splitConsole, "Split console Shortcut is working again.");
});
