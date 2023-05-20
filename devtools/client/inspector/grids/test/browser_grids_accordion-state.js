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
const GRID_PANE_SELECTOR = "#layout-grid-section";
const ACCORDION_HEADER_SELECTOR = ".accordion-header";
const ACCORDION_CONTENT_SELECTOR = ".accordion-content";

add_task(async function () {
  await addTab("data:text/html;charset=utf-8," + encodeURIComponent(TEST_URI));
  const { inspector, gridInspector, toolbox } = await openLayoutView();
  const { document: doc } = gridInspector;

  await testAccordionStateAfterClickingHeader(doc);
  await testAccordionStateAfterSwitchingSidebars(inspector, doc);
  await testAccordionStateAfterReopeningLayoutView(toolbox);

  Services.prefs.clearUserPref(GRID_OPENED_PREF);
});

async function testAccordionStateAfterClickingHeader(doc) {
  const item = await waitFor(() => doc.querySelector(GRID_PANE_SELECTOR));
  const header = item.querySelector(ACCORDION_HEADER_SELECTOR);
  const content = item.querySelector(ACCORDION_CONTENT_SELECTOR);

  info("Checking initial state of the grid panel.");
  ok(
    !content.hidden && content.childElementCount > 0,
    "The grid panel content is visible."
  );
  ok(
    Services.prefs.getBoolPref(GRID_OPENED_PREF),
    `${GRID_OPENED_PREF} is pref on by default.`
  );

  info("Clicking the grid header to hide the grid panel.");
  header.click();

  info("Checking the new state of the grid panel.");
  ok(content.hidden, "The grid panel content is hidden.");
  ok(
    !Services.prefs.getBoolPref(GRID_OPENED_PREF),
    `${GRID_OPENED_PREF} is pref off.`
  );
}

async function testAccordionStateAfterSwitchingSidebars(inspector, doc) {
  info(
    "Checking the grid accordion state is persistent after switching sidebars."
  );

  info("Selecting the computed view.");
  inspector.sidebar.select("computedview");

  info("Selecting the layout view.");
  inspector.sidebar.select("layoutview");

  const item = await waitFor(() => doc.querySelector(GRID_PANE_SELECTOR));
  const content = item.querySelector(ACCORDION_CONTENT_SELECTOR);

  info("Checking the state of the grid panel.");
  ok(content.hidden, "The grid panel content is hidden.");
  ok(
    !Services.prefs.getBoolPref(GRID_OPENED_PREF),
    `${GRID_OPENED_PREF} is pref off.`
  );
}

async function testAccordionStateAfterReopeningLayoutView(toolbox) {
  info(
    "Checking the grid accordion state is persistent after closing and re-opening the " +
      "layout view."
  );

  info("Closing the toolbox.");
  await toolbox.destroy();

  info("Re-opening the layout view.");
  const { gridInspector } = await openLayoutView();
  const { document: doc } = gridInspector;

  const item = await waitFor(() => doc.querySelector(GRID_PANE_SELECTOR));
  const content = item.querySelector(ACCORDION_CONTENT_SELECTOR);

  info("Checking the state of the grid panel.");
  ok(content.hidden, "The grid panel content is hidden.");
  ok(
    !Services.prefs.getBoolPref(GRID_OPENED_PREF),
    `${GRID_OPENED_PREF} is pref off.`
  );
}
