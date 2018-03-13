/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the split console state is persisted.

const TEST_URI = "data:text/html;charset=utf-8,<p>Web Console test for splitting</p>";

add_task(async function() {
  info("Opening a tab while there is no user setting on split console pref");
  let toolbox = await openNewTabAndToolbox(TEST_URI, "inspector");
  ok(!toolbox.splitConsole, "Split console is hidden by default");
  ok(!isCommandButtonChecked(toolbox), "Split console button is unchecked by default.");

  await toggleSplitConsoleWithEscape(toolbox);
  ok(toolbox.splitConsole, "Split console is now visible.");
  ok(isCommandButtonChecked(toolbox), "Split console button is now checked.");
  ok(getVisiblePrefValue(), "Visibility pref is true");

  is(getHeightPrefValue(), toolbox.webconsolePanel.height,
     "Panel height matches the pref");
  toolbox.webconsolePanel.height = 200;

  await toolbox.destroy();

  info("Opening a tab while there is a true user setting on split console pref");
  toolbox = await openNewTabAndToolbox(TEST_URI, "inspector");
  ok(toolbox.splitConsole, "Split console is visible by default.");

  ok(isCommandButtonChecked(toolbox), "Split console button is checked by default.");
  is(getHeightPrefValue(), 200, "Height is set based on panel height after closing");

  let activeElement = getActiveElement(toolbox.doc);
  let inputNode = toolbox.getPanel("webconsole").hud.jsterm.inputNode;
  is(activeElement, inputNode, "Split console input is focused by default");

  toolbox.webconsolePanel.height = 1;
  ok(toolbox.webconsolePanel.clientHeight > 1,
     "The actual height of the console is bound with a min height");

  toolbox.webconsolePanel.height = 10000;
  ok(toolbox.webconsolePanel.clientHeight < 10000,
     "The actual height of the console is bound with a max height");

  await toggleSplitConsoleWithEscape(toolbox);
  ok(!toolbox.splitConsole, "Split console is now hidden.");
  ok(!isCommandButtonChecked(toolbox), "Split console button is now unchecked.");
  ok(!getVisiblePrefValue(), "Visibility pref is false");

  await toolbox.destroy();

  is(getHeightPrefValue(), 10000, "Height is set based on panel height after closing");

  info("Opening a tab while there is a false user setting on split " +
       "console pref");
  toolbox = await openNewTabAndToolbox(TEST_URI, "inspector");

  ok(!toolbox.splitConsole, "Split console is hidden by default.");
  ok(!getVisiblePrefValue(), "Visibility pref is false");

  await toolbox.destroy();
});

function getActiveElement(doc) {
  let activeElement = doc.activeElement;
  while (activeElement && activeElement.contentDocument) {
    activeElement = activeElement.contentDocument.activeElement;
  }
  return activeElement;
}

function getVisiblePrefValue() {
  return Services.prefs.getBoolPref("devtools.toolbox.splitconsoleEnabled");
}

function getHeightPrefValue() {
  return Services.prefs.getIntPref("devtools.toolbox.splitconsoleHeight");
}

function isCommandButtonChecked(toolbox) {
  return toolbox.doc.querySelector("#command-button-splitconsole")
    .classList.contains("checked");
}

function toggleSplitConsoleWithEscape(toolbox) {
  let onceSplitConsole = toolbox.once("split-console");
  let toolboxWindow = toolbox.win;
  toolboxWindow.focus();
  EventUtils.sendKey("ESCAPE", toolboxWindow);
  return onceSplitConsole;
}
