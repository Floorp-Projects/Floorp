/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that keyboard interaction works fine with the table widget

const TEST_URI = "data:text/xml;charset=UTF-8,<?xml version='1.0'?>" +
  "<?xml-stylesheet href='chrome://global/skin/global.css'?>" +
  "<?xml-stylesheet href='chrome://browser/skin/devtools/common.css'?>" +
  "<?xml-stylesheet href='chrome://browser/skin/devtools/light-theme.css'?>" +
  "<?xml-stylesheet href='chrome://browser/skin/devtools/widgets.css'?>" +
  "<window xmlns='http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul'" +
  " title='Table Widget' width='600' height='500'>" +
  "<box flex='1' class='theme-light'/></window>";
const TEST_OPT = "chrome,titlebar,toolbar,centerscreen,resizable,dialog=no";

const {TableWidget} = require("devtools/shared/widgets/TableWidget");
var {Task} = require("resource://gre/modules/Task.jsm");

var doc, table;

function test() {
  waitForExplicitFinish();
  let win = Services.ww.openWindow(null, TEST_URI, "_blank", TEST_OPT, null);

  win.addEventListener("load", function onLoad() {
    win.removeEventListener("load", onLoad, false);

    waitForFocus(function () {
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
  });
}

function endTests() {
  table.destroy();
  doc.defaultView.close();
  doc = table = null;
  finish();
}

var startTests = Task.async(function*() {
  populateTable();
  yield testKeyboardInteraction();
  endTests();
});

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
    executeSoon(() => EventUtils.synthesizeMouseAtCenter(node, {}, doc.defaultView));
  } else {
    executeSoon(() => EventUtils.synthesizeMouseAtCenter(node, {
      button: button,
      type: "contextmenu"
    }, doc.defaultView));
  }
}

/**
 * Tests if pressing navigation keys on the table items does the expected behavior
 */
var testKeyboardInteraction = Task.async(function*() {
  info("Testing keyboard interaction with the table");
  info("clicking on first row");
  let node = table.tbody.firstChild.firstChild.children[1];
  let event = table.once(TableWidget.EVENTS.ROW_SELECTED);
  click(node);
  let [name, id] = yield event;

  node = table.tbody.firstChild.firstChild.children[2];
  // node should not have selected class
  ok(!node.classList.contains("theme-selected"),
     "Row should not have selected class");
  info("Pressing down key to select next row");
  event = table.once(TableWidget.EVENTS.ROW_SELECTED);
  EventUtils.sendKey("DOWN", doc.defaultView);
  id = yield event;
  is(id, "id2", "Correct row was selected after pressing down");
  ok(node.classList.contains("theme-selected"), "row has selected class");
  let nodes = doc.querySelectorAll(".theme-selected");
  for (let i = 0; i < nodes.length; i++) {
    is(nodes[i].getAttribute("data-id"), "id2",
       "Correct cell selected in all columns");
  }

  node = table.tbody.firstChild.firstChild.children[3];
  // node should not have selected class
  ok(!node.classList.contains("theme-selected"),
     "Row should not have selected class");
  info("Pressing down key to select next row");
  event = table.once(TableWidget.EVENTS.ROW_SELECTED);
  EventUtils.sendKey("DOWN", doc.defaultView);
  id = yield event;
  is(id, "id3", "Correct row was selected after pressing down");
  ok(node.classList.contains("theme-selected"), "row has selected class");
  nodes = doc.querySelectorAll(".theme-selected");
  for (let i = 0; i < nodes.length; i++) {
    is(nodes[i].getAttribute("data-id"), "id3",
       "Correct cell selected in all columns");
  }

  // pressing up arrow key to select previous row
  node = table.tbody.firstChild.firstChild.children[2];
  // node should not have selected class
  ok(!node.classList.contains("theme-selected"),
     "Row should not have selected class");
  info("Pressing up key to select previous row");
  event = table.once(TableWidget.EVENTS.ROW_SELECTED);
  EventUtils.sendKey("UP", doc.defaultView);
  id = yield event;
  is(id, "id2", "Correct row was selected after pressing down");
  ok(node.classList.contains("theme-selected"), "row has selected class");
  nodes = doc.querySelectorAll(".theme-selected");
  for (let i = 0; i < nodes.length; i++) {
    is(nodes[i].getAttribute("data-id"), "id2",
       "Correct cell selected in all columns");
  }

  // selecting last item node to test edge navigation cycling case
  table.selectedRow = "id9";
  // pressing down now should move to first row.
  node = table.tbody.firstChild.firstChild.children[1];
  // node should not have selected class
  ok(!node.classList.contains("theme-selected"),
     "Row should not have selected class");
  info("Pressing down key on last row to select first row");
  event = table.once(TableWidget.EVENTS.ROW_SELECTED);
  EventUtils.sendKey("DOWN", doc.defaultView);
  id = yield event;
  is(id, "id1", "Correct row was selected after pressing down");
  ok(node.classList.contains("theme-selected"), "row has selected class");
  nodes = doc.querySelectorAll(".theme-selected");
  for (let i = 0; i < nodes.length; i++) {
    is(nodes[i].getAttribute("data-id"), "id1",
       "Correct cell selected in all columns");
  }

  // pressing up now should move to last row.
  node = table.tbody.firstChild.firstChild.lastChild;
  // node should not have selected class
  ok(!node.classList.contains("theme-selected"),
     "Row should not have selected class");
  info("Pressing down key on last row to select first row");
  event = table.once(TableWidget.EVENTS.ROW_SELECTED);
  EventUtils.sendKey("UP", doc.defaultView);
  id = yield event;
  is(id, "id9", "Correct row was selected after pressing down");
  ok(node.classList.contains("theme-selected"), "row has selected class");
  nodes = doc.querySelectorAll(".theme-selected");
  for (let i = 0; i < nodes.length; i++) {
    is(nodes[i].getAttribute("data-id"), "id9",
       "Correct cell selected in all columns");
  }
});
