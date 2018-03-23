/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Tests that the grid's accordion state is persistent through hide/show in the layout
// view.

const TEST_URI = `
  <style type='text/css'>
    #grid {
      display: grid;
    }
  </style>
  <div id="grid">
    <div id="cell1">cell1</div>
    <div id="cell2">cell2</div>
  </div>
`;

const GRID_OPENED_PREF = "devtools.layout.grid.opened";

add_task(function* () {
  yield addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  let { inspector, gridInspector, toolbox } = yield openLayoutView();
  let { document: doc } = gridInspector;

  yield testAccordionStateAfterClickingHeader(doc);
  yield testAccordionStateAfterSwitchingSidebars(inspector, doc);
  yield testAccordionStateAfterReopeningLayoutView(toolbox);

  Services.prefs.clearUserPref(GRID_OPENED_PREF);
});

function* testAccordionStateAfterClickingHeader(doc) {
  let header = doc.querySelector(".grid-pane ._header");
  let gContent = doc.querySelector(".grid-pane ._content");

  info("Checking initial state of the grid panel.");
  is(gContent.style.display, "block", "The grid panel content is 'display: block'.");
  ok(Services.prefs.getBoolPref(GRID_OPENED_PREF),
    `${GRID_OPENED_PREF} is pref on by default.`);

  info("Clicking the grid header to hide the grid panel.");
  header.click();

  info("Checking the new state of the grid panel.");
  is(gContent.style.display, "none", "The grid panel content is 'display: none'.");
  ok(!Services.prefs.getBoolPref(GRID_OPENED_PREF), `${GRID_OPENED_PREF} is pref off.`);
}

function* testAccordionStateAfterSwitchingSidebars(inspector, doc) {
  info("Checking the grid accordion state is persistent after switching sidebars.");

  let gContent = doc.querySelector(".grid-pane ._content");

  info("Selecting the computed view.");
  inspector.sidebar.select("computedview");

  info("Selecting the layout view.");
  inspector.sidebar.select("layoutview");

  info("Checking the state of the grid panel.");
  is(gContent.style.display, "none", "The grid panel content is 'display: none'.");
  ok(!Services.prefs.getBoolPref(GRID_OPENED_PREF), `${GRID_OPENED_PREF} is pref off.`);
}

function* testAccordionStateAfterReopeningLayoutView(toolbox) {
  info("Checking the grid accordion state is persistent after closing and re-opening the "
  + "layout view.");

  info("Closing the toolbox.");
  yield toolbox.destroy();

  info("Re-opening the layout view.");
  let { gridInspector } = yield openLayoutView();
  let { document: doc } = gridInspector;
  let gContent = doc.querySelector(".grid-pane ._content");

  info("Checking the state of the grid panel.");
  ok(!gContent, "The grid panel content is not rendered.");
  ok(!Services.prefs.getBoolPref(GRID_OPENED_PREF),
    `${GRID_OPENED_PREF} is pref off.`);
}
