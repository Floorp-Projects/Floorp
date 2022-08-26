/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

requestLongerTimeout(2);

// Check that the stylesheets list can be filtered

const INITIAL_INLINE_STYLE_SHEETS_COUNT = 100;

const TEST_URI =
  "data:text/html;charset=UTF-8," +
  encodeURIComponent(
    `
      <!DOCTYPE html>
      <html>
        <head>
          <title>Test filter</title>
          <link rel="stylesheet" type="text/css" href="${TEST_BASE_HTTPS}simple.css">
          ${Array.from({ length: INITIAL_INLINE_STYLE_SHEETS_COUNT })
            .map((_, i) => `<style>/* inline ${i} */</style>`)
            .join("\n")}
          <link rel="stylesheet" type="text/css" href="${TEST_BASE_HTTPS}pretty.css">
        </head>
        <body>
        </body>
      </html>
    `
  );

add_task(async function() {
  const { panel, ui } = await openStyleEditorForURL(TEST_URI);
  const { panelWindow } = panel;
  is(
    ui.editors.length,
    INITIAL_INLINE_STYLE_SHEETS_COUNT + 2,
    "correct number of editors"
  );

  const doc = panel.panelWindow.document;

  const filterInput = doc.querySelector(".devtools-filterinput");
  const filterInputClearButton = doc.querySelector(
    ".devtools-searchinput-clear"
  );
  ok(filterInput, "There's a filter input");
  ok(filterInputClearButton, "There's a clear button next to the filter input");
  ok(
    filterInputClearButton.hasAttribute("hidden"),
    "The clear button is hidden by default"
  );

  const setFilterInputValue = value => {
    // The keyboard shortcut focuses the input and select its content, so we should
    // be able to type right-away.
    synthesizeKeyShortcut("CmdOrCtrl+P");
    EventUtils.sendString(value);
  };

  info(
    "Check that the list can be filtered with the stylesheet name, regardless of the casing"
  );
  let onEditorSelected = ui.once("editor-selected");
  setFilterInputValue("PREttY");
  ok(
    !filterInputClearButton.hasAttribute("hidden"),
    "The clear button is visible when the input isn't empty"
  );
  Assert.deepEqual(
    getVisibleStyleSheetsNames(doc),
    ["pretty.css"],
    "Only pretty.css is now displayed"
  );

  await onEditorSelected;
  is(
    ui.selectedEditor,
    ui.editors.at(-1),
    "When the selected stylesheet is filtered out, the first visible one gets selected"
  );
  is(
    filterInput.ownerGlobal.document.activeElement,
    filterInput,
    "Even when a stylesheet was automatically opened, the filter input is still focused"
  );
  ok(!ui.selectedEditor.sourceEditor.hasFocus(), "Editor doesn't have focus.");

  info(
    "Clicking on the clear button should clear the input and unfilter the list"
  );
  EventUtils.synthesizeMouseAtCenter(
    filterInputClearButton,
    {},
    panel.panelWindow
  );
  is(filterInput.value, "", "input was cleared");
  ok(!isListFiltered(doc), "List isn't filtered anymore");
  ok(
    filterInputClearButton.hasAttribute("hidden"),
    "The clear button is hidden after clicking on it"
  );

  info("Check that the list can be filtered with name-less stylesheets");
  onEditorSelected = ui.once("editor-selected");
  setFilterInputValue("#1");
  Assert.deepEqual(
    getVisibleStyleSheetsNames(doc),
    [
      "<inline style sheet #1>",
      "<inline style sheet #10>",
      "<inline style sheet #11>",
      "<inline style sheet #12>",
      "<inline style sheet #13>",
      "<inline style sheet #14>",
      "<inline style sheet #15>",
      "<inline style sheet #16>",
      "<inline style sheet #17>",
      "<inline style sheet #18>",
      "<inline style sheet #19>",
      "<inline style sheet #100>",
    ],
    `List is showing inline stylesheets whose index start with "1"`
  );
  await onEditorSelected;
  is(
    ui.selectedEditor,
    ui.editors[1],
    "The first visible stylesheet got selected"
  );

  info("Check that keyboard navigation still works when the list is filtered");
  // Move focus out of the input
  EventUtils.synthesizeKey("VK_TAB", {}, panelWindow);
  EventUtils.synthesizeKey("VK_DOWN", {}, panelWindow);

  is(
    panelWindow.document.activeElement.childNodes[0].value,
    "<inline style sheet #1>",
    "focus is on first inline stylesheet"
  );

  EventUtils.synthesizeKey("VK_DOWN", {}, panelWindow);
  is(
    panelWindow.document.activeElement.childNodes[0].value,
    "<inline style sheet #10>",
    "focus is on inline stylesheet #10"
  );

  EventUtils.synthesizeKey("VK_DOWN", {}, panelWindow);
  is(
    panelWindow.document.activeElement.childNodes[0].value,
    "<inline style sheet #11>",
    "focus is on inline stylesheet #11"
  );

  info(
    "Check that when stylesheets are added in the page, they respect the filter state"
  );
  let onEditorAdded = ui.once("editor-added");
  // Adding an inline stylesheet that will match the search
  await SpecialPowers.spawn(gBrowser.selectedBrowser, [], () => {
    const document = content.document;
    const style = document.createElement("style");
    style.appendChild(document.createTextNode(`/* inline 101 */`));
    document.head.appendChild(style);
  });
  await onEditorAdded;
  ok(
    getVisibleStyleSheetsNames(doc).includes("<inline style sheet #101>"),
    "New inline stylesheet is visible as it matches the search"
  );

  // Adding a stylesheet that won't match the search
  onEditorAdded = ui.once("editor-added");
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [TEST_BASE_HTTPS],
    baseUrl => {
      const document = content.document;
      const link = document.createElement("link");
      link.setAttribute("rel", "stylesheet");
      link.setAttribute("type", "text/css");
      link.setAttribute("href", `${baseUrl}doc_short_string.css`);
      document.head.appendChild(link);
    }
  );
  await onEditorAdded;

  ok(
    !getVisibleStyleSheetsNames(doc).includes("doc_short_string.css"),
    "doc_short_string.css is not visible as its name does not match the search"
  );

  info(
    "Check that clicking on the Add New Stylesheet button clears the list and show the stylesheet"
  );
  onEditorAdded = ui.once("editor-added");
  await createNewStyleSheet(ui, panel.panelWindow);
  is(filterInput.value, "", "Filter input was cleared");

  ok(!isListFiltered(doc), "List is not filtered anymore");
  is(ui.selectedEditor, ui.editors.at(-1), "The new stylesheet got selected");

  info(
    "Check that when no stylesheet matches the search, a class is added to the nav"
  );
  setFilterInputValue("sync_with_csp");
  ok(navHasAllFilteredClass(panel), `"splitview-all-filtered" was added`);
  ok(
    filterInput
      .closest(".devtools-searchbox")
      .classList.contains("devtools-searchbox-no-match"),
    `The searchbox has the "devtools-searchbox-no-match" class`
  );

  info(
    "Check that adding a stylesheet matching the search remove the splitview-all-filtered class"
  );
  onEditorAdded = ui.once("editor-added");
  await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [TEST_BASE_HTTPS],
    baseUrl => {
      const document = content.document;
      const link = document.createElement("link");
      link.setAttribute("rel", "stylesheet");
      link.setAttribute("type", "text/css");
      link.setAttribute("href", `${baseUrl}sync_with_csp.css`);
      document.head.appendChild(link);
    }
  );
  await onEditorAdded;
  ok(!navHasAllFilteredClass(panel), `"splitview-all-filtered" was removed`);
  ok(
    !filterInput
      .closest(".devtools-searchbox")
      .classList.contains("devtools-searchbox-no-match"),
    `The searchbox does not have the "devtools-searchbox-no-match" class anymore`
  );

  info(
    "Check that reloading the page when the filter don't match anything won't select anything"
  );
  setFilterInputValue("XXXDONTMATCHANYTHING");
  ok(navHasAllFilteredClass(panel), `"splitview-all-filtered" was added`);
  await reloadPageAndWaitForStyleSheets(
    ui,
    INITIAL_INLINE_STYLE_SHEETS_COUNT + 2
  );
  ok(
    navHasAllFilteredClass(panel),
    `"splitview-all-filtered" is still applied`
  );
  is(getVisibleStyleSheets(doc).length, 0, "No stylesheets are displayed");
  is(ui.selectedEditor, null, "No editor was selected");

  info(
    "Check that reloading the page when the filter was matching elements keep the same state"
  );
  onEditorSelected = ui.once("editor-selected");
  setFilterInputValue("pretty");
  await onEditorSelected;
  Assert.deepEqual(
    getVisibleStyleSheetsNames(doc),
    ["pretty.css"],
    "Only pretty.css is now displayed"
  );

  onEditorSelected = ui.once("editor-selected");
  await reloadPageAndWaitForStyleSheets(
    ui,
    INITIAL_INLINE_STYLE_SHEETS_COUNT + 2
  );
  await onEditorSelected;
  Assert.deepEqual(
    getVisibleStyleSheetsNames(doc),
    ["pretty.css"],
    "pretty.css is still the only stylesheet displayed"
  );
  is(
    ui.selectedEditor.friendlyName,
    "pretty.css",
    "pretty.css editor is active"
  );

  info("Check that clearing the input does show all the stylesheets");
  EventUtils.synthesizeMouseAtCenter(
    filterInputClearButton,
    {},
    panel.panelWindow
  );
  ok(!isListFiltered(doc), "List is not filtered anymore");
  is(
    ui.selectedEditor.friendlyName,
    "pretty.css",
    "pretty.css editor is still active"
  );
});

/**
 * @param {StyleEditorPanel} panel
 * @returns Boolean
 */
function navHasAllFilteredClass(panel) {
  return panel.panelWindow.document
    .querySelector(".splitview-nav")
    .classList.contains("splitview-all-filtered");
}

/**
 * Returns true if there's at least one stylesheet filtered out
 *
 * @param {Document} doc: StyleEditor document
 * @returns Boolean
 */
function isListFiltered(doc) {
  return !!doc.querySelectorAll("ol > li.splitview-filtered").length;
}

/**
 * Returns the list of stylesheet list elements.
 *
 * @param {Document} doc: StyleEditor document
 * @returns Array<Element>
 */
function getVisibleStyleSheets(doc) {
  return Array.from(
    doc.querySelectorAll(
      "ol > li:not(.splitview-filtered) .stylesheet-name label"
    )
  );
}

/**
 * Returns the list of stylesheet names visible in the style editor list.
 *
 * @param {Document} doc: StyleEditor document
 * @returns Array<String>
 */
function getVisibleStyleSheetsNames(doc) {
  return getVisibleStyleSheets(doc).map(label => label.value);
}
