/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the flexbox's accordion state is persistent through hide/show in the layout
// view.

const TEST_URI = URL_ROOT + "doc_flexbox_specific_cases.html";

const FLEXBOX_OPENED_PREF = "devtools.layout.flexbox.opened";

add_task(async function() {
  await addTab(TEST_URI);
  const { inspector, flexboxInspector, toolbox } = await openLayoutView();
  const { document: doc } = flexboxInspector;

  await testAccordionStateAfterClickingHeader(doc);
  await testAccordionStateAfterSwitchingSidebars(inspector, doc);
  await testAccordionStateAfterReopeningLayoutView(toolbox);

  Services.prefs.clearUserPref(FLEXBOX_OPENED_PREF);
});

function testAccordionStateAfterClickingHeader(doc) {
  const header = doc.querySelector(".flex-accordion ._header");
  const content = doc.querySelector(".flex-accordion ._content");

  info("Checking initial state of the flexbox panel.");
  is(
    content.style.display,
    "block",
    "The flexbox panel content is 'display: block'."
  );
  ok(
    Services.prefs.getBoolPref(FLEXBOX_OPENED_PREF),
    `${FLEXBOX_OPENED_PREF} is pref on by default.`
  );

  info("Clicking the flexbox header to hide the flexbox panel.");
  header.click();

  info("Checking the new state of the flexbox panel.");
  is(
    content.style.display,
    "none",
    "The flexbox panel content is 'display: none'."
  );
  ok(
    !Services.prefs.getBoolPref(FLEXBOX_OPENED_PREF),
    `${FLEXBOX_OPENED_PREF} is pref off.`
  );
}

function testAccordionStateAfterSwitchingSidebars(inspector, doc) {
  info(
    "Checking the flexbox accordion state is persistent after switching sidebars."
  );

  const content = doc.querySelector(".flex-accordion ._content");

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
  ok(
    !Services.prefs.getBoolPref(FLEXBOX_OPENED_PREF),
    `${FLEXBOX_OPENED_PREF} is pref off.`
  );
}

async function testAccordionStateAfterReopeningLayoutView(toolbox) {
  info(
    "Checking the flexbox accordion state is persistent after closing and re-opening " +
      "the layout view."
  );

  info("Closing the toolbox.");
  await toolbox.destroy();

  info("Re-opening the layout view.");
  const { flexboxInspector } = await openLayoutView();
  const { document: doc } = flexboxInspector;
  const content = doc.querySelector(".flex-accordion ._content");

  info("Checking the state of the flexbox panel.");
  ok(!content, "The flexbox panel content is not rendered.");
  ok(
    !Services.prefs.getBoolPref(FLEXBOX_OPENED_PREF),
    `${FLEXBOX_OPENED_PREF} is pref off.`
  );
}
