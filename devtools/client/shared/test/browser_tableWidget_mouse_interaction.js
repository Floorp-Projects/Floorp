/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that mosue interaction works fine with the table widget

"use strict";

const TEST_URI = CHROME_URL_ROOT + "doc_tableWidget_mouse_interaction.xul";
const TEST_OPT = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

const {TableWidget} = require("devtools/client/shared/widgets/TableWidget");

var doc, table;

function test() {
  waitForExplicitFinish();
  const win = Services.ww.openWindow(null, TEST_URI, "_blank", TEST_OPT, null);

  win.addEventListener("load", function() {
    waitForFocus(function() {
      doc = win.document;
      table = new TableWidget(doc.querySelector("box"), {
        initialColumns: {
          col1: "Column 1",
          col2: "Column 2",
          col3: "Column 3",
          col4: "Column 4",
        },
        uniqueId: "col1",
        emptyText: "This is dummy empty text",
        highlightUpdated: true,
        removableColumns: true,
        wrapTextInElements: true,
      });
      startTests();
    });
  }, {once: true});
}

function endTests() {
  table.destroy();
  doc.defaultView.close();
  doc = table = null;
  finish();
}

var startTests = async function() {
  populateTable();
  await testMouseInteraction();
  endTests();
};

function populateTable() {
  table.push({
    col1: "id1",
    col2: "value10",
    col3: "value20",
    col4: "value30",
  });
  table.push({
    col1: "id2",
    col2: "value14",
    col3: "value29",
    col4: "value32",
  });
  table.push({
    col1: "id3",
    col2: "value17",
    col3: "value21",
    col4: "value31",
    extraData: "foobar",
    extraData2: 42,
  });
  table.push({
    col1: "id4",
    col2: "value12",
    col3: "value26",
    col4: "value33",
  });
  table.push({
    col1: "id5",
    col2: "value19",
    col3: "value26",
    col4: "value37",
  });
  table.push({
    col1: "id6",
    col2: "value15",
    col3: "value25",
    col4: "value37",
  });
  table.push({
    col1: "id7",
    col2: "value18",
    col3: "value21",
    col4: "value36",
    somethingExtra: "Hello World!",
  });
  table.push({
    col1: "id8",
    col2: "value11",
    col3: "value27",
    col4: "value34",
  });
  table.push({
    col1: "id9",
    col2: "value11",
    col3: "value23",
    col4: "value38",
  });
}

// Sends a click event on the passed DOM node in an async manner
function click(node, button = 0) {
  if (button == 0) {
    executeSoon(() => EventUtils.synthesizeMouseAtCenter(node, {},
                                                         doc.defaultView));
  } else {
    executeSoon(() => EventUtils.synthesizeMouseAtCenter(node, {
      button: button,
      type: "contextmenu",
    }, doc.defaultView));
  }
}

/**
 * Tests if clicking the table items does the expected behavior
 */
var testMouseInteraction = async function() {
  info("Testing mouse interaction with the table");
  ok(!table.selectedRow, "Nothing should be selected beforehand");

  let event = table.once(TableWidget.EVENTS.ROW_SELECTED);
  const firstColumnFirstRowCell = table.tbody.firstChild.firstChild.children[1];
  info("clicking on the first row");
  ok(!firstColumnFirstRowCell.classList.contains("theme-selected"),
     "Node should not have selected class before clicking");
  click(firstColumnFirstRowCell);
  let id = await event;
  ok(firstColumnFirstRowCell.classList.contains("theme-selected"),
     "Node has selected class after click");
  is(id, "id1", "Correct row was selected");

  info("clicking on second row to select it");
  event = table.once(TableWidget.EVENTS.ROW_SELECTED);
  const firstColumnSecondRowCell = table.tbody.firstChild.firstChild.children[2];
  // node should not have selected class
  ok(!firstColumnSecondRowCell.classList.contains("theme-selected"),
     "New node should not have selected class before clicking");
  click(firstColumnSecondRowCell);
  id = await event;
  ok(firstColumnSecondRowCell.classList.contains("theme-selected"),
     "New node has selected class after clicking");
  is(id, "id2", "Correct table path is emitted for new node");
  isnot(firstColumnFirstRowCell, firstColumnSecondRowCell,
    "Old and new node are different");
  ok(!firstColumnFirstRowCell.classList.contains("theme-selected"),
     "Old node should not have selected class after the click on new node");

  info("clicking on the third row cell content to select third row");
  event = table.once(TableWidget.EVENTS.ROW_SELECTED);
  const firstColumnThirdRowCell = table.tbody.firstChild.firstChild.children[3];
  const firstColumnThirdRowCellInnerNode = firstColumnThirdRowCell.querySelector("span");
  // node should not have selected class
  ok(!firstColumnThirdRowCell.classList.contains("theme-selected"),
     "New node should not have selected class before clicking");
  click(firstColumnThirdRowCellInnerNode);
  id = await event;
  ok(firstColumnThirdRowCell.classList.contains("theme-selected"),
     "New node has selected class after clicking the cell content");
  is(id, "id3", "Correct table path is emitted for new node");

  // clicking on table header to sort by it
  event = table.once(TableWidget.EVENTS.COLUMN_SORTED);
  let node = table.tbody.children[6].firstChild.children[0];
  info("clicking on the 4th coulmn header to sort the table by it");
  ok(!node.hasAttribute("sorted"),
     "Node should not have sorted attribute before clicking");
  ok(doc.querySelector("[sorted]"),
     "Although, something else should be sorted on");
  isnot(doc.querySelector("[sorted]"), node, "Which is not equal to this node");
  click(node);
  id = await event;
  is(id, "col4", "Correct column was sorted on");
  ok(node.hasAttribute("sorted"),
     "Node should now have sorted attribute after clicking");
  is(doc.querySelectorAll("[sorted]").length, 1,
     "Now only one column should be sorted on");
  is(doc.querySelector("[sorted]"), node, "Which should be this column");

  // test context menu opening.
  // hiding second column
  // event listener for popupshown
  info("right click on the first column header");
  node = table.tbody.firstChild.firstChild.firstChild;
  let onPopupShown = once(table.menupopup, "popupshown");
  click(node, 2);
  await onPopupShown;

  is(table.menupopup.querySelectorAll("menuitem[disabled]").length, 1,
     "Only 1 menuitem is disabled");
  is(table.menupopup.querySelector("menuitem[disabled]"),
     table.menupopup.querySelector("[data-id='col1']"),
     "Which is the unique column");

  // popup should be open now
  // clicking on second column label
  let onPopupHidden = once(table.menupopup, "popuphidden");
  event = table.once(TableWidget.EVENTS.HEADER_CONTEXT_MENU);
  node = table.menupopup.querySelector("[data-id='col2']");
  info("selecting to hide the second column");
  ok(!table.tbody.children[2].hasAttribute("hidden"),
     "Column is not hidden before hiding it");
  click(node);
  id = await event;
  await onPopupHidden;
  is(id, "col2", "Correct column was triggered to be hidden");
  is(table.tbody.children[2].getAttribute("hidden"), "true",
     "Column is hidden after hiding it");

  // hiding third column
  // event listener for popupshown
  info("right clicking on the first column header");
  node = table.tbody.firstChild.firstChild.firstChild;
  onPopupShown = once(table.menupopup, "popupshown");
  click(node, 2);
  await onPopupShown;

  is(table.menupopup.querySelectorAll("menuitem[disabled]").length, 1,
     "Only 1 menuitem is disabled");
  // popup should be open now
  // clicking on second column label
  onPopupHidden = once(table.menupopup, "popuphidden");
  event = table.once(TableWidget.EVENTS.HEADER_CONTEXT_MENU);
  node = table.menupopup.querySelector("[data-id='col3']");
  info("selecting to hide the second column");
  ok(!table.tbody.children[4].hasAttribute("hidden"),
     "Column is not hidden before hiding it");
  click(node);
  id = await event;
  await onPopupHidden;
  is(id, "col3", "Correct column was triggered to be hidden");
  is(table.tbody.children[4].getAttribute("hidden"), "true",
     "Column is hidden after hiding it");

  // opening again to see if 2 items are disabled now
  // event listener for popupshown
  info("right clicking on the first column header");
  node = table.tbody.firstChild.firstChild.firstChild;
  onPopupShown = once(table.menupopup, "popupshown");
  click(node, 2);
  await onPopupShown;

  is(table.menupopup.querySelectorAll("menuitem[disabled]").length, 2,
     "2 menuitems are disabled now as only 2 columns remain visible");
  is(table.menupopup.querySelectorAll("menuitem[disabled]")[0],
     table.menupopup.querySelector("[data-id='col1']"),
     "First is the unique column");
  is(table.menupopup.querySelectorAll("menuitem[disabled]")[1],
     table.menupopup.querySelector("[data-id='col4']"),
     "Second is the last column");

  // showing back 2nd column
  // popup should be open now
  // clicking on second column label
  onPopupHidden = once(table.menupopup, "popuphidden");
  event = table.once(TableWidget.EVENTS.HEADER_CONTEXT_MENU);
  node = table.menupopup.querySelector("[data-id='col2']");
  info("selecting to hide the second column");
  is(table.tbody.children[2].getAttribute("hidden"), "true",
     "Column is hidden before unhiding it");
  click(node);
  id = await event;
  await onPopupHidden;
  is(id, "col2", "Correct column was triggered to be hidden");
  ok(!table.tbody.children[2].hasAttribute("hidden"),
     "Column is not hidden after unhiding it");

  // showing back 3rd column
  // event listener for popupshown
  info("right clicking on the first column header");
  node = table.tbody.firstChild.firstChild.firstChild;
  onPopupShown = once(table.menupopup, "popupshown");
  click(node, 2);
  await onPopupShown;

  // popup should be open now
  // clicking on second column label
  onPopupHidden = once(table.menupopup, "popuphidden");
  event = table.once(TableWidget.EVENTS.HEADER_CONTEXT_MENU);
  node = table.menupopup.querySelector("[data-id='col3']");
  info("selecting to hide the second column");
  is(table.tbody.children[4].getAttribute("hidden"), "true",
     "Column is hidden before unhiding it");
  click(node);
  id = await event;
  await onPopupHidden;
  is(id, "col3", "Correct column was triggered to be hidden");
  ok(!table.tbody.children[4].hasAttribute("hidden"),
     "Column is not hidden after unhiding it");

  // reset table state
  table.clearSelection();
  table.sortBy("col1");
};
