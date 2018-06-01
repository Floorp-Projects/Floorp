/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that keyboard interaction works fine with the table widget

"use strict";

const TEST_URI = CHROME_URL_ROOT + "doc_tableWidget_keyboard_interaction.xul";
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
          col4: "Column 4"
        },
        uniqueId: "col1",
        emptyText: "This is dummy empty text",
        highlightUpdated: true,
        removableColumns: true,
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
  await testKeyboardInteraction();
  endTests();
};

function populateTable() {
  table.push({
    col1: "id1",
    col2: "value10",
    col3: "value20",
    col4: "value30"
  });
  table.push({
    col1: "id2",
    col2: "value14",
    col3: "value29",
    col4: "value32"
  });
  table.push({
    col1: "id3",
    col2: "value17",
    col3: "value21",
    col4: "value31",
    extraData: "foobar",
    extraData2: 42
  });
  table.push({
    col1: "id4",
    col2: "value12",
    col3: "value26",
    col4: "value33"
  });
  table.push({
    col1: "id5",
    col2: "value19",
    col3: "value26",
    col4: "value37"
  });
  table.push({
    col1: "id6",
    col2: "value15",
    col3: "value25",
    col4: "value37"
  });
  table.push({
    col1: "id7",
    col2: "value18",
    col3: "value21",
    col4: "value36",
    somethingExtra: "Hello World!"
  });
  table.push({
    col1: "id8",
    col2: "value11",
    col3: "value27",
    col4: "value34"
  });
  table.push({
    col1: "id9",
    col2: "value11",
    col3: "value23",
    col4: "value38"
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
      type: "contextmenu"
    }, doc.defaultView));
  }
}

function getNodeByValue(value) {
  return table.tbody.querySelector("[value=" + value + "]");
}

/**
 * Tests if pressing navigation keys on the table items does the expected
 * behavior.
 */
var testKeyboardInteraction = async function() {
  info("Testing keyboard interaction with the table");
  info("clicking on the row containing id2");
  const node = getNodeByValue("id2");
  const event = table.once(TableWidget.EVENTS.ROW_SELECTED);
  click(node);
  await event;

  await testRow("id3", "DOWN", "next row");
  await testRow("id4", "DOWN", "next row");
  await testRow("id3", "UP", "previous row");
  await testRow("id4", "DOWN", "next row");
  await testRow("id5", "DOWN", "next row");
  await testRow("id6", "DOWN", "next row");
  await testRow("id5", "UP", "previous row");
  await testRow("id4", "UP", "previous row");
  await testRow("id3", "UP", "previous row");

  // selecting last item node to test edge navigation cycling case
  table.selectedRow = "id9";

  // pressing down on last row should move to first row.
  await testRow("id1", "DOWN", "first row");

  // pressing up now should move to last row.
  await testRow("id9", "UP", "last row");
};

async function testRow(id, key, destination) {
  const node = getNodeByValue(id);
  // node should not have selected class
  ok(!node.classList.contains("theme-selected"),
     "Row should not have selected class");
  info(`Pressing ${key} to select ${destination}`);

  const event = table.once(TableWidget.EVENTS.ROW_SELECTED);
  EventUtils.sendKey(key, doc.defaultView);

  const uniqueId = await event;
  is(id, uniqueId, `Correct row was selected after pressing ${key}`);

  ok(node.classList.contains("theme-selected"), "row has selected class");

  const nodes = doc.querySelectorAll(".theme-selected");
  for (let i = 0; i < nodes.length; i++) {
    is(nodes[i].getAttribute("data-id"), id,
       "Correct cell selected in all columns");
  }
}
