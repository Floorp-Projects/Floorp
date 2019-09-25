/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flexbox accordions state is persistent through hide/show in the layout
// view.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

add_task(async function() {
  await addTab(TEST_URI);

  await testAccordionState(
    ":root",
    FLEXBOX_OPENED_PREF,
    ".flex-accordion-none"
  );
  await testAccordionState(
    "#container-only",
    FLEX_CONTAINER_OPENED_PREF,
    ".flex-accordion-container"
  );
  await testAccordionState(
    "#item-only",
    FLEX_ITEM_OPENED_PREF,
    ".flex-accordion-item"
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

  const header = await waitFor(() => doc.querySelector(selector + " ._header"));
  const content = await waitFor(() =>
    doc.querySelector(selector + " ._content")
  );

  is(
    content.style.display,
    "block",
    "The flexbox panel content is 'display: block'."
  );
  ok(Services.prefs.getBoolPref(pref), `${pref} is pref on by default.`);

  info("Clicking the flexbox header to hide the flexbox panel.");
  header.click();

  info("Checking the new state of the flexbox panel.");
  is(
    content.style.display,
    "none",
    "The flexbox panel content is 'display: none'."
  );
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

  const content = await waitFor(() =>
    doc.querySelector(selector + " ._content")
  );

  info("Selecting the computed view.");
  inspector.sidebar.select("computedview");

  info("Selecting the layout view.");
  inspector.sidebar.select("layoutview");

  info("Checking the state of the flexbox panel.");
  is(
    content.style.display,
    "none",
    "The flexbox panel content is 'display: none'."
  );
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
  const content = await waitFor(() =>
    doc.querySelector(selector + " ._content")
  );

  info("Checking the state of the flexbox panel.");
  is(content.style.display, "none", "The flexbox panel content is hidden.");
  is(content.children.length, 0, "The flexbox panel content is not rendered.");
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
