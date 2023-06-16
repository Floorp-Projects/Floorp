/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flexbox accordions state is persistent through hide/show in the layout
// view.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";
const ACCORDION_HEADER_SELECTOR = ".accordion-header";
const ACCORDION_CONTENT_SELECTOR = ".accordion-content";

add_task(async function () {
  await addTab(TEST_URI);

  await testAccordionState(
    ":root",
    FLEXBOX_OPENED_PREF,
    "#layout-section-flex"
  );
  await testAccordionState(
    "#container-only",
    FLEX_CONTAINER_OPENED_PREF,
    "#layout-section-flex-container"
  );
  await testAccordionState(
    "#item-only",
    FLEX_ITEM_OPENED_PREF,
    "#layout-section-flex-item"
  );
});

async function testAccordionState(target, pref, selector) {
  const context = await openLayoutViewAndSelectNode(target);

  await testAccordionStateAfterClickingHeader(pref, selector, context);
  await testAccordionStateAfterSwitchingSidebars(pref, selector, context);
  await testAccordionStateAfterReopeningLayoutView(pref, selector, context);

  Services.prefs.clearUserPref(pref);
}

async function testAccordionStateAfterClickingHeader(pref, selector, { doc }) {
  info("Checking initial state of the flexbox panel.");

  const item = await waitFor(() => doc.querySelector(selector));
  const header = item.querySelector(ACCORDION_HEADER_SELECTOR);
  const content = item.querySelector(ACCORDION_CONTENT_SELECTOR);

  ok(
    !content.hidden && content.childElementCount > 0,
    "The flexbox panel content is visible."
  );
  ok(Services.prefs.getBoolPref(pref), `${pref} is pref on by default.`);

  info("Clicking the flexbox header to hide the flexbox panel.");
  header.click();

  info("Checking the new state of the flexbox panel.");
  ok(content.hidden, "The flexbox panel content is hidden.");
  ok(!Services.prefs.getBoolPref(pref), `${pref} is pref off.`);
}

async function testAccordionStateAfterSwitchingSidebars(
  pref,
  selector,
  { doc, inspector }
) {
  info(
    "Checking the flexbox accordion state is persistent after switching sidebars."
  );

  const item = await waitFor(() => doc.querySelector(selector));

  info("Selecting the computed view.");
  inspector.sidebar.select("computedview");

  info("Selecting the layout view.");
  inspector.sidebar.select("layoutview");

  info("Checking the state of the flexbox panel.");
  const content = item.querySelector(ACCORDION_CONTENT_SELECTOR);

  ok(content.hidden, "The flexbox panel content is hidden.");
  ok(!Services.prefs.getBoolPref(pref), `${pref} is pref off.`);
}

async function testAccordionStateAfterReopeningLayoutView(
  pref,
  selector,
  { target, toolbox }
) {
  info(
    "Checking the flexbox accordion state is persistent after closing and re-opening the layout view."
  );

  info("Closing the toolbox.");
  await toolbox.destroy();

  info("Re-opening the layout view.");
  const { doc } = await openLayoutViewAndSelectNode(target);
  const item = await waitFor(() => doc.querySelector(selector));
  const content = item.querySelector(ACCORDION_CONTENT_SELECTOR);

  info("Checking the state of the flexbox panel.");
  ok(content.hidden, "The flexbox panel content is hidden.");
  ok(!Services.prefs.getBoolPref(pref), `${pref} is pref off.`);
}

async function openLayoutViewAndSelectNode(target) {
  const { inspector, flexboxInspector, toolbox } = await openLayoutView();
  await selectNode(target, inspector);

  return {
    doc: flexboxInspector.document,
    inspector,
    target,
    toolbox,
  };
}
