/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

// Test that 'no styles' indicator is shown if a page doesn't contain any style
// sheets.

const TESTCASE_URI = TEST_BASE_HTTP + "nostyle.html";

add_task(async function() {
  // Make enough room for the "append style sheet" link to not wrap,
  // as it messes up with EvenEventUtils.synthesizeMouse
  await pushPref("devtools.styleeditor.navSidebarWidth", 500);
  const { panel, ui } = await openStyleEditorForURL(TESTCASE_URI);
  const { panelWindow } = panel;

  ok(
    !getRootElement(panel).classList.contains("loading"),
    "style editor root element does not have 'loading' class name anymore"
  );

  const newButton = panelWindow.document.querySelector(
    "toolbarbutton.style-editor-newButton"
  );
  ok(!newButton.hasAttribute("disabled"), "new style sheet button is enabled");

  const importButton = panelWindow.document.querySelector(
    ".style-editor-importButton"
  );
  ok(!importButton.hasAttribute("disabled"), "import button is enabled");

  const emptyPlaceHolderEl = getRootElement(panel).querySelector(
    ".empty.placeholder"
  );
  isnot(
    emptyPlaceHolderEl.ownerGlobal.getComputedStyle(emptyPlaceHolderEl).display,
    "none",
    "showing 'no style' indicator"
  );

  info(
    "Check that clicking on the append new stylesheet link do add a stylesheet"
  );
  const onEditorAdded = ui.once("editor-added");
  const newLink = emptyPlaceHolderEl.querySelector("a.style-editor-newButton");

  // Use synthesizeMouse to also check that the element is visible
  EventUtils.synthesizeMouseAtCenter(newLink, {}, newLink.ownerGlobal);
  await onEditorAdded;

  ok(true, "A stylesheet was added");
  is(
    emptyPlaceHolderEl.ownerGlobal.getComputedStyle(emptyPlaceHolderEl).display,
    "none",
    "The empty placeholder element is now hidden"
  );
});
