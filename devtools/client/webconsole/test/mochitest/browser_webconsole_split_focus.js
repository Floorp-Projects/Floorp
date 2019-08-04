/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,<p>Web Console test for splitting</p>";

add_task(async function() {
  info(
    "Test that the split console input is focused and restores the focus properly."
  );

  const toolbox = await openNewTabAndToolbox(TEST_URI, "inspector");
  ok(!toolbox.splitConsole, "Split console is hidden by default");

  info("Focusing the search box before opening the split console");
  const inspector = toolbox.getPanel("inspector");
  inspector.searchBox.focus();

  let activeElement = getActiveElement(inspector.panelDoc);
  is(activeElement, inspector.searchBox, "Search box is focused");

  await toolbox.openSplitConsole();

  ok(toolbox.splitConsole, "Split console is now visible");

  const { hud } = toolbox.getPanel("webconsole");
  ok(isInputFocused(hud), "Split console input is focused by default");

  await toolbox.closeSplitConsole();

  info(
    "Making sure that the search box is refocused after closing the split console"
  );
  activeElement = getActiveElement(inspector.panelDoc);
  is(activeElement, inspector.searchBox, "Search box is focused");
});

function getActiveElement(doc) {
  let activeElement = doc.activeElement;
  while (activeElement && activeElement.contentDocument) {
    activeElement = activeElement.contentDocument.activeElement;
  }
  return activeElement;
}
